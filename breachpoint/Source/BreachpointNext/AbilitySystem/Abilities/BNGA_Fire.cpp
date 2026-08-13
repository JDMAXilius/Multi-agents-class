#include "AbilitySystem/Abilities/BNGA_Fire.h"

#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "BreachpointNext.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Weapons/BNWeapon.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_ServerWaitClientTargetData.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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
	return ASC && !ASC->HasMatchingGameplayTag(BNTags::State_Weapon_Reloading);
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

	if (!ActorInfo->IsLocallyControlled())
	{
		// The authority's instance for a remote shooter: it fires nothing itself and NEVER ends
		// itself on the ordinary paths. The client's EndAbility replicates ServerEndAbility, and
		// reliable RPCs on one channel arrive in order — so the last shot's TargetData is always
		// judged BEFORE this instance dies. An authority that ended on its own flow would destroy
		// the wait task while the claim was still in flight and throw every Single shot away.
		// The backstop if the shooter never ends it: UBNEquipmentComponent::EndPlay revokes the
		// weapon's set on pawn destruction, and clearing the spec cancels this instance.
		UAbilityTask_ServerWaitClientTargetData* Wait = UAbilityTask_ServerWaitClientTargetData::ServerWaitForClientTargetData(this, NAME_None, /*TriggerOnce=*/false);
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
	if (!bRepeats || Row->FireDelay <= 0.f || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
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
	const APlayerController* PC = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	if (!Row || !ASC || !World || !PC)
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
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
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
		World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ShotDir * Row->Range, ECC_Visibility, QueryParams);
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

	// The server does NOT trust the hit. It re-derives the shot from its own view of the world and
	// keeps only what that view can account for: no more pellets than the row allows; a real
	// victim that is neither the shooter nor its own weapon; an impact inside the row's Range of
	// the SERVER's view point; an impact inside the row's cone off the SERVER's aim direction; and
	// an impact sitting on the claimed actor's bounds AS THE SERVER SEES THEM, so a client cannot
	// name a victim it is nowhere near.
	// KNOWN LIMIT, recorded rather than hidden: there is no lag-compensated rewind this wave, so a
	// target is judged at its CURRENT server position — a fast mover is forgiving inside tolerance.
	FVector ViewLocation;
	FRotator ViewRotation;
	if (const APlayerController* PC = ActorInfo->PlayerController.Get())
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
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

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FGameplayAbilityTargetData* Data = TargetData.Get(Index);
		const FHitResult* Hit = Data ? Data->GetHitResult() : nullptr;
		if (!Hit)
		{
			continue;
		}

		// On a MISS ImpactPoint is (0,0,0) and TraceEnd is the far end of the shot — the tracer
		// must reach the latter (MyCharacter.cpp:1885-1890 learned this by firing at the sky and
		// watching the tracer fly to the world origin). Clamped to the server's own range so the
		// end point is never further than the weapon could reach.
		FVector TracerEnd = Hit->bBlockingHit ? Hit->ImpactPoint : Hit->TraceEnd;
		const FVector ToEnd = TracerEnd - ViewLocation;
		if (ToEnd.SizeSquared() > FMath::Square(MaxRange))
		{
			TracerEnd = ViewLocation + ToEnd.GetSafeNormal() * MaxRange;
		}

		FGameplayCueParameters TracerParams;
		TracerParams.Location = TracerEnd;
		TracerParams.Instigator = Avatar;
		TracerParams.SourceObject = Weapon;
		K2_ExecuteGameplayCueWithParams(BNTags::GameplayCue_Weapon_Tracer, TracerParams);

		AActor* HitActor = Hit->GetActor();
		if (!Hit->bBlockingHit || !IsValid(HitActor) || HitActor == Avatar || HitActor == Weapon)
		{
			continue;
		}

		const FVector ToImpact = Hit->ImpactPoint - ViewLocation;
		if (ToImpact.SizeSquared() > FMath::Square(MaxRange))
		{
			continue;
		}
		const double AimDot = FMath::Clamp(FVector::DotProduct(ToImpact.GetSafeNormal(), ServerAim), -1.0, 1.0);
		if (FMath::RadiansToDegrees(FMath::Acos(AimDot)) > MaxAngle)
		{
			continue;
		}
		if (!HitActor->GetComponentsBoundingBox(true).ExpandBy(BNValidationDistanceTolerance).IsInside(Hit->ImpactPoint))
		{
			continue;
		}

		UE_LOG(LogBN, Log, TEXT("BNGA_Fire: validated hit — %s hit %s at %s, %.0f uu. No damage this wave; G5 owns the one damage door."),
			*GetNameSafe(Avatar), *GetNameSafe(HitActor), *Hit->ImpactPoint.ToCompactString(), ToImpact.Size());

		// G5's damage call goes HERE and nothing else about this ability changes. No engine damage
		// API is reachable from this file — there is no second path to unbuild later.

		FGameplayCueParameters ImpactParams;
		ImpactParams.Location = Hit->ImpactPoint;
		ImpactParams.Normal = Hit->ImpactNormal;
		ImpactParams.PhysicalMaterial = Hit->PhysMaterial.Get();
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

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
