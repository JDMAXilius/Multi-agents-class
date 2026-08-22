#include "AbilitySystem/Abilities/BNGA_Fire.h"

#include "AbilitySystem/Effects/BNDamage.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "BreachpointNext.h"
#include "Characters/BNCharacter.h"
#include "Core/BNCollision.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Weapons/BNWeapon.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Tasks/BNAbilityTask_ServerWaitClientTargetData.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

namespace
{
	// What the server allows itself when judging a claim. Not gameplay tuning: they are the price
	// of a round trip, so they live beside the check that spends them.
	constexpr float BNValidationDistanceTolerance = 200.f;
	constexpr float BNValidationAngleTolerance = 15.f;
	// A claim arriving sooner than half the nominal period is not jitter, it is a rate hack.
	constexpr float BNRateFloorFraction = 0.5f;

	ABNWeapon* BNFireGetWeapon(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const ABNCharacter* Character = ActorInfo ? Cast<ABNCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		const UBNEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
		return Equipment ? Equipment->GetCurrentWeapon() : nullptr;
	}

	const FBNWeaponRow* BNFireGetRow(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const ABNWeapon* Weapon = BNFireGetWeapon(ActorInfo);
		return Weapon ? Weapon->GetRow() : nullptr;
	}
}

bool UBNGA_Fire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Reload BLOCKS fire; fire does not cancel reload. The magazine goes 0 -> full at one instant
	// this wave, so a shot that interrupted a reload would be a shot with no defined ammo state.
	// A partial-magazine model is what would make interrupting meaningful, and it does not exist.
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return ASC
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Weapon_Reloading)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Weapon_Melee);
}

bool UBNGA_Fire::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	const ABNWeapon* Weapon = BNFireGetWeapon(ActorInfo);
	return Weapon && Weapon->HasAmmo(1);
}

void UBNGA_Fire::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	// One round per trigger pull regardless of ShotCount: pellets are one shot's spread, not
	// several shots. Authority-only inside ConsumeAmmo, so the client's call here is a no-op.
	if (ABNWeapon* Weapon = BNFireGetWeapon(ActorInfo))
	{
		Weapon->ConsumeAmmo(1);
	}
}

const FGameplayTagContainer* UBNGA_Fire::GetCooldownTags() const
{
	// Built on first use rather than in the constructor: native tags are not guaranteed
	// registered while CDOs are being built — the reason UBNGA_Lean resolves its tag in a virtual.
	if (CooldownTags.IsEmpty())
	{
		CooldownTags.AddTag(BNTags::Cooldown_Weapon_Fire);
	}
	return &CooldownTags;
}

void UBNGA_Fire::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const FBNWeaponRow* Row = BNFireGetRow(ActorInfo);
	if (!Row || Row->FireDelay <= 0.f)
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UBNGE_FireCooldown::StaticClass(), GetAbilityLevel());
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(BNSetByCaller::FireDelay, Row->FireDelay);
	Spec.Data->DynamicGrantedTags.AddTag(BNTags::Cooldown_Weapon_Fire);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UBNGA_Fire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Cost (one round) and cooldown (the row's FireDelay) are both paid HERE, inside GAS's commit
	// path. A rejection therefore consumes nothing and leaves no cooldown stuck: the client never
	// wrote ammo, and its predicted cooldown GE is keyed to the rejected prediction key, which GAS
	// discards with the key.
	// RECORDED, not fixed: the server spends shot 0's round on the ACTIVATION, before any claim
	// arrives, so a player tap-spamming fire drains their own magazine even if no TargetData ever
	// follows. Self-harm again — the shot is paid for, it simply produces no hit — and moving the
	// charge onto the claim would put the cost outside the commit path this packet was told to
	// keep it in.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FBNWeaponRow* Row = BNFireGetRow(ActorInfo);
	if (!Row)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ShotsFired = 0;
	ShotsJudged = 0;
	LastAcceptedShotTime = 0.f;

	// The ability IS the firing state. Applied on every role so Mixed replication carries
	// State.Weapon.Firing to simulated proxies — NewMoons flipped a replicated bool on the pawn
	// instead, which is the path this replaces.
	FiringHandle = ApplyStateTag(BNTags::State_Weapon_Firing);

	if (!ActorInfo->IsLocallyControlled())
	{
		// The authority's instance for a remote shooter: it fires nothing itself and NEVER ends
		// itself on the ordinary paths. The client's EndAbility replicates ServerEndAbility, and
		// reliable RPCs on one channel arrive in order — so the last shot's TargetData is always
		// judged BEFORE this instance dies. An authority that ended on its own flow would destroy
		// the wait task while the claim was still in flight and throw every Single shot away.
		// The backstop if the shooter never ends it: UBNEquipmentComponent::EndPlay revokes the
		// weapon's set on pawn destruction, and clearing the spec cancels this instance.
		UBNAbilityTask_ServerWaitClientTargetData* Wait = UBNAbilityTask_ServerWaitClientTargetData::ServerWaitForClientTargetData(this, NAME_None, /*TriggerOnce=*/false);
		Wait->ValidData.AddDynamic(this, &UBNGA_Fire::OnTargetDataReceived);
		Wait->ReadyForActivation();

		// The first shot announced to every OTHER machine now, off the activation itself, rather
		// than waiting for the claim — the shooter already predicted its own.
		PlayShotEffects();
		return;
	}

	FireShot();

	// Cadence, mined from MyCharacter::StartFire (.cpp:1678-1717): Single fires once with no
	// timer; Auto fires immediately then loops at FireDelay for as long as the trigger is held;
	// Burst counts to BurstShotCount and stops itself (BurstFireTick, .cpp:1719-1729). What is NOT
	// mined is its component plumbing — the timer lives on this ability instance and dies with it.
	const bool bRepeats = (Row->FireMode == EBNFireMode::Auto)
		|| (Row->FireMode == EBNFireMode::Burst && Row->BurstShotCount > 1);
	UWorld* World = GetWorld();
	if (Row->FireDelay <= 0.f || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!bRepeats)
	{
		// Single: ending same-frame would apply and strip State.Weapon.Firing before any anim
		// update saw it. Lifetime is the row's FireDelay — the same period cooldown already owns.
		World->GetTimerManager().SetTimer(FireTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}), Row->FireDelay, /*bLoop=*/false);
		return;
	}

	World->GetTimerManager().SetTimer(FireTimer, this, &UBNGA_Fire::FireTick, Row->FireDelay, /*bLoop=*/true);

	if (Row->FireMode == EBNFireMode::Auto)
	{
		UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
		ReleaseTask->OnRelease.AddDynamic(this, &UBNGA_Fire::OnInputRelease);
		ReleaseTask->ReadyForActivation();
	}
}

void UBNGA_Fire::FireTick()
{
	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	const FBNWeaponRow* Row = BNFireGetRow(ActorInfo);

	// A destroyed avatar answers null here, so the loop cannot outlive its pawn on the persistent
	// PlayerState ASC — Wave 2's lesson, the same guard UBNGA_Sprint::CheckGate carries. CheckCost
	// reads the replicated magazine, so an empty weapon stops the loop on the shooter's own screen
	// rather than firing rounds the server will refuse.
	const bool bAlive = ActorInfo && ActorInfo->AvatarActor.IsValid() && Row;
	const bool bBurstDone = bAlive && Row->FireMode == EBNFireMode::Burst && ShotsFired >= FMath::Max(1, Row->BurstShotCount);
	if (!bAlive || bBurstDone || !CheckCost(CurrentSpecHandle, ActorInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	FireShot();
}

void UBNGA_Fire::PlayShotEffects()
{
	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	ABNWeapon* Weapon = BNFireGetWeapon(ActorInfo);
	const FBNWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!Row || !ASC)
	{
		return;
	}

	// Law 2: all FX through cues. Called inside the activation's prediction key on both roles, so
	// the shooter's own flash is immediate AND the authority's multicast of the same cue skips the
	// shooter and reaches everyone else, simulated proxies included — one flash per machine, never
	// two. (GAS drops the multicast for the client whose prediction key it carries.)
	FGameplayCueParameters MuzzleParams;
	MuzzleParams.Instigator = ActorInfo->AvatarActor;
	MuzzleParams.SourceObject = Weapon;
	K2_ExecuteGameplayCueWithParams(BNTags::GameplayCue_Weapon_MuzzleFlash, MuzzleParams);

	// Through the ASC, not the mesh's AnimInstance: the ASC replicates its montage, so simulated
	// proxies see the shot too. The ability may end before the montage does — ClearAnimatingAbility
	// only drops the ownership pointer, it does not stop playback.
	if (UAnimMontage* Montage = Row->FireMontage.IsNull() ? nullptr : Row->FireMontage.LoadSynchronous())
	{
		ASC->PlayMontage(this, CurrentActivationInfo, Montage, 1.f);
	}
}

void UBNGA_Fire::FireShot()
{
	++ShotsFired;

	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	ABNWeapon* Weapon = BNFireGetWeapon(ActorInfo);
	const FBNWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = GetWorld();
	// The pawn's controller, not ActorInfo->PlayerController: null for a bot, and an
	// AIController's GetPlayerViewPoint is the pawn's eye view — a PlayerController's is unchanged.
	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AController* ViewController = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	if (!Row || !ASC || !World || !ViewController)
	{
		return;
	}

	// Every shot, repeats included, runs under the activation's prediction key — that is what
	// makes the cue predict locally and makes the server's copy of it skip the shooter.
	FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());

	PlayShotEffects();

	// The aim ray is the CONTROLLER's view point, not the camera component. MyCharacter::GetAimRay
	// (.h:107-112, .cpp:1817-1862) documents why the hard way: the camera component's world
	// transform sits at the character's FEET and carries the mesh's yaw/roll, so tracing from it
	// sent every shot into the floor. The view point IS the crosshair.
	FVector ViewLocation;
	FRotator ViewRotation;
	ViewController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector AimDir = ViewRotation.Vector();

	FCollisionQueryParams QueryParams(FName(TEXT("BNWeaponFire")), /*bTraceComplex=*/false, ActorInfo->AvatarActor.Get());
	QueryParams.AddIgnoredActor(Weapon);

	// ShotCount pellets through SpreadAngle, which is the cone's HALF-angle — the template's
	// RandomUnitVectorInConeInDegrees(Dir, SpreadAngle) (MyCharacter.cpp:1668-1672), same cone.
	FGameplayAbilityTargetDataHandle TargetData;
	const int32 Pellets = FMath::Max(1, Row->ShotCount);
	const float ConeRadians = FMath::DegreesToRadians(FMath::Max(0.f, Row->SpreadAngle));
	for (int32 Pellet = 0; Pellet < Pellets; ++Pellet)
	{
		const FVector ShotDir = (ConeRadians > 0.f) ? FMath::VRandCone(AimDir, ConeRadians) : AimDir;

		FHitResult Hit;
		World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ShotDir * Row->Range, BNCollision::WeaponTrace, QueryParams);
		TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
	}

	if (ActorInfo->IsNetAuthority())
	{
		// Listen-server host firing its own pawn: it already holds the truth, so judge it here
		// rather than RPC the claim to ourselves.
		OnTargetDataReceived(TargetData);
		return;
	}

	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), TargetData, FGameplayTag(), ASC->ScopedPredictionKey);
}

void UBNGA_Fire::OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& TargetData)
{
	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	ABNWeapon* Weapon = BNFireGetWeapon(ActorInfo);
	const FBNWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = GetWorld();
	if (!Avatar || !Row || !ASC || !World)
	{
		return;
	}

	// Consumed per claim: TriggerOnce is false, so without this the stored handle accumulates
	// every shot of the activation and the next one re-judges all of them.
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	// The RATE is the server's, not the client's: the shooter owns cadence but cannot beat this
	// floor, so a client sending a hundred claims in one frame pays for one shot and is ignored
	// ninety-nine times.
	// RECORDED, not fixed: this is a wall-clock delta, not a per-claim budget. A client hitch can
	// queue several honest claims that then dispatch inside one server frame, and every one after
	// the first is dropped — after the shooter has already seen its own muzzle flash. It only ever
	// costs the hitching player their own shots, so it is a self-harm, not an exploit.
	const float Now = World->GetTimeSeconds();
	if (ShotsJudged > 0 && Row->FireDelay > 0.f && (Now - LastAcceptedShotTime) < Row->FireDelay * BNRateFloorFraction)
	{
		return;
	}

	// Shot 0 of the activation was already paid for by CommitAbility. Every repeat pays here,
	// through the same CheckCost/ApplyCost the commit path uses — the authority is the only
	// machine that ever moves the magazine.
	if (ShotsJudged > 0)
	{
		if (!CheckCost(CurrentSpecHandle, ActorInfo))
		{
			return;
		}
		ApplyCost(CurrentSpecHandle, ActorInfo, CurrentActivationInfo);
		ApplyCooldown(CurrentSpecHandle, ActorInfo, CurrentActivationInfo);

		// A remote shooter's repeat, announced to every other machine. A listen-server host played
		// its own inside FireShot, so it must not play a second one here.
		if (!ActorInfo->IsLocallyControlled())
		{
			PlayShotEffects();
		}
	}
	++ShotsJudged;
	LastAcceptedShotTime = Now;

	// The client's TargetData is a CLAIM. The only thing it is allowed to assert is WHICH actor it
	// hit and roughly where; the server produces the hit itself and everything downstream — impact
	// point, normal, physical material, actor, cue locations and G5's damage line — is built from
	// the SERVER's own FHitResult. Nothing client-supplied survives into the authoritative result.
	//
	// Order: cheap pre-filters first (pellet count, real victim, range, cone, bounds), then the
	// server's own line trace along the claimed direction. Without that trace an enemy behind a
	// wall passes every pre-filter — it is inside the cone, inside range, and the claimed point is
	// inside its bounds — and the shot would be validated straight through geometry. That is a
	// wallhack the moment G5 lands damage on this line.
	//
	// This is CONFIRMATION, not lag-compensated rewind: the server traces against the world as it
	// stands now, so the stated limit is unchanged — a target is judged at its CURRENT server
	// position and a fast mover stays forgiving inside tolerance.
	FVector ViewLocation;
	FRotator ViewRotation;
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	if (const AController* ViewController = AvatarPawn ? AvatarPawn->GetController() : nullptr)
	{
		// The pawn's controller: a PlayerController answers the crosshair unchanged, an
		// AIController answers the pawn's eye view — same seam as FireShot's.
		ViewController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		ViewLocation = Avatar->GetActorLocation();
		ViewRotation = Avatar->GetActorRotation();
	}

	const FVector ServerAim = ViewRotation.Vector();
	const float MaxRange = Row->Range + BNValidationDistanceTolerance;
	const float MaxAngle = FMath::Max(0.f, Row->SpreadAngle) + BNValidationAngleTolerance;
	const int32 Count = FMath::Min(TargetData.Num(), FMath::Max(1, Row->ShotCount));

	FCollisionQueryParams QueryParams(FName(TEXT("BNWeaponConfirm")), /*bTraceComplex=*/false, Avatar);
	QueryParams.AddIgnoredActor(Weapon);
	// The impact cue picks its per-surface row off Parameters.PhysicalMaterial; without this
	// the hit carries none and every impact falls back to the default surface.
	QueryParams.bReturnPhysicalMaterial = true;

	// The validated hit is NOT predicted state, so its cues must not ride the shooter's prediction
	// key. A multicast carrying that key is skipped on the machine that GENERATED it — which is
	// exactly the shooter — so tracer and impact would reach every machine except the one that
	// fired. An explicit empty key makes the multicast reach the shooter too, and it is still the
	// only execution on any machine because nothing predicts these locally (unlike MuzzleFlash,
	// which the shooter does predict and therefore must keep on the activation key).
	FScopedPredictionWindow UnpredictedCues(ASC, FPredictionKey());

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FGameplayAbilityTargetData* Data = TargetData.Get(Index);
		const FHitResult* Claim = Data ? Data->GetHitResult() : nullptr;
		if (!Claim)
		{
			continue;
		}

		// On a MISS ImpactPoint is (0,0,0) and TraceEnd is the far end of the shot — the shot's
		// direction has to come from the latter (MyCharacter.cpp:1885-1890 learned this by firing
		// at the sky and watching the tracer fly to the world origin).
		const FVector ClaimPoint = Claim->bBlockingHit ? Claim->ImpactPoint : Claim->TraceEnd;
		const FVector ToClaim = ClaimPoint - ViewLocation;
		if (ToClaim.SizeSquared() > FMath::Square(MaxRange))
		{
			continue;
		}
		const double AimDot = FMath::Clamp(FVector::DotProduct(ToClaim.GetSafeNormal(), ServerAim), -1.0, 1.0);
		if (FMath::RadiansToDegrees(FMath::Acos(AimDot)) > MaxAngle)
		{
			continue;
		}

		// The server's own shot, along the claimed direction, at the row's range, on the same
		// channel, ignoring the shooter and its weapon. From here on nothing reads Claim except
		// the ONE thing the client is allowed to assert: which actor it says it hit.
		FHitResult ServerHit;
		World->LineTraceSingleByChannel(ServerHit, ViewLocation, ViewLocation + ToClaim.GetSafeNormal() * Row->Range, BNCollision::WeaponTrace, QueryParams);

		FGameplayCueParameters TracerParams;
		TracerParams.Location = ServerHit.bBlockingHit ? ServerHit.ImpactPoint : ServerHit.TraceEnd;
		TracerParams.Instigator = Avatar;
		TracerParams.SourceObject = Weapon;
		K2_ExecuteGameplayCueWithParams(BNTags::GameplayCue_Weapon_Tracer, TracerParams);

		// A claimed miss stays a miss: the server does not award a hit the shooter never asserted.
		AActor* ClaimedActor = Claim->GetActor();
		if (!Claim->bBlockingHit || !IsValid(ClaimedActor))
		{
			continue;
		}

		// The claimed victim must be within its own bounds of the claimed point AS THE SERVER SEES
		// THEM, and — the check that closes the wall — must be what the server's trace hits FIRST.
		if (!ClaimedActor->GetComponentsBoundingBox(true).ExpandBy(BNValidationDistanceTolerance).IsInside(ClaimPoint))
		{
			continue;
		}

		AActor* HitActor = ServerHit.GetActor();
		if (!ServerHit.bBlockingHit || HitActor != ClaimedActor || HitActor == Avatar || HitActor == Weapon)
		{
			continue;
		}

		const float ServerDistance = FVector::Dist(ServerHit.ImpactPoint, ViewLocation);
		UE_LOG(LogBN, Log, TEXT("BNGA_Fire: validated hit — %s hit %s at %s, %.0f uu."),
			*GetNameSafe(Avatar), *GetNameSafe(HitActor), *ServerHit.ImpactPoint.ToCompactString(), ServerDistance);

		// THE one damage door, and the only call to it in this file. Built from the SERVER's own
		// ServerHit — never the client's Claim — and from the ROW, which is where the damage and
		// the headshot multiplier live. No engine damage API is reachable from here, so there is
		// no second path to unbuild when the real pipeline replaces BNDamage's insides.
		BNDamage::ApplyWeaponDamage(Avatar, Weapon ? Weapon->GetRowName() : NAME_None, *Row, ServerHit);

		FGameplayCueParameters ImpactParams;
		ImpactParams.Location = ServerHit.ImpactPoint;
		ImpactParams.Normal = ServerHit.ImpactNormal;
		ImpactParams.PhysicalMaterial = ServerHit.PhysMaterial.Get();
		ImpactParams.Instigator = Avatar;
		ImpactParams.SourceObject = Weapon;
		K2_ExecuteGameplayCueWithParams(BNTags::GameplayCue_Weapon_Impact, ImpactParams);
	}
}

void UBNGA_Fire::OnInputRelease(float TimeHeld)
{
	// Only the shooter's own machine reaches this; the authority's instance ends when the
	// shooter's EndAbility replicates, which is what keeps the last claim judgeable.
	if (CurrentActorInfo && CurrentActorInfo->IsLocallyControlled())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UBNGA_Fire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}
	ShotsFired = 0;
	ShotsJudged = 0;
	LastAcceptedShotTime = 0.f;
	RemoveStateTag(FiringHandle);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
