// BREACHPOINT — BP03 step 2. The fire path.

#include "AbilitySystem/Abilities/BRGA_WeaponFire.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "Data/BRDataRows.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Weapons/BREquipmentComponent.h"
#include "Weapons/BRWeaponInstance.h"

namespace
{

	/**
	 * The bone whose hits count as headshots. STRUCTURAL, not tuning: it names a skeleton
	 * socket, not a balance figure, so it does not belong in `DT_Weapons`. The MULTIPLIER is
	 * tuning and lives in the row (`HeadshotMult`) and `CT_Combat`, applied by the exec calc —
	 * this constant only decides whether the tag is added.
	 */
	const FName HeadBoneName(TEXT("head"));

	/** How far past `Range_m` a claim may land before it is rejected. Absorbs the difference
	 *  between the client's muzzle and the server's rewound one; it does NOT absorb a cheat,
	 *  because it is a fixed small distance rather than a fraction of range. */
	constexpr float RangeToleranceUU = 50.f;
}

UBRGA_WeaponFire::UBRGA_WeaponFire(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// InstancedPerActor + LocalPredicted: the shooter feels the shot immediately, the server
	// decides whether it happened. Anything a player feels is predicted (`gas-purity.md` §4).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	// The ability's ASSET tag. This is what a future ability lists to cancel firing, and it is
	// the ONLY thing CancelAbilitiesWithTag matches against. `SetAssetTags` is the 5.5+
	// spelling; the `AbilityTags` property it replaces is deprecated and documented as becoming
	// private — BRGA_Sprint already established this and it is the reason this compiles.
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Weapon_Fire);
	SetAssetTags(AssetTags);

	// Firing ends a sprint, Halo-style, with zero code in either ability. Note this lists
	// `Ability.Sprint` — the ABILITY's asset tag — and NOT `State.Movement.Sprinting`, which is
	// a tag on the ACTOR. ARCHITECTURE §3.3's sentence says the latter and does not work;
	// BRGameplayTags.h records that correction and this is the code that depends on it.
	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	// Rate of fire is a cooldown, so automatic fire needs no Tick and no timer (law 4): the
	// input buffer re-activates and CheckCooldown decides whether it may.
	CooldownTag = BRGameplayTags::Ability_Weapon_Fire;

	bCommitOnActivate = true;
}

// ================================================================================================
// Preconditions
// ================================================================================================

bool UBRGA_WeaponFire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UBRWeaponInstance* Weapon = GetActiveWeapon();
	if (!Weapon)
	{
		return false;
	}

	// Ammo is the named GAS exception: weapon state, not an attribute, so it is checked here
	// rather than as a GE cost (`gas-purity.md` §8).
	if (Weapon->GetAmmoInMag() <= 0)
	{
		return false;
	}

	const FBRWeaponRow* Row = Weapon->GetRow();
	if (!Row)
	{
		// No row means the DataTable is not imported (BP18 step 3) or the handle is stale.
		// Refusing is correct: a shot with no row has no damage, no range and no cue.
		return false;
	}

	// A projectile weapon must not be hitscanned. Refuse loudly rather than firing the wrong
	// delivery — the rocket is BP09's ability and has a travel time this path cannot express.
	if (Row->DamageDelivery != EBRDamageDelivery::Hitscan)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGA_WeaponFire refused '%s': DamageDelivery is not Hitscan. A projectile "
				 "weapon needs its own ability (BP09), not this one."),
			*Weapon->GetWeaponRowName().ToString());
		return false;
	}

	if (Row->Range_m <= 0.f)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGA_WeaponFire refused '%s': Range_m is %.2f. A hitscan trace needs a length, "
				 "and inventing one here would be a law-3 violation."),
			*Weapon->GetWeaponRowName().ToString(), Row->Range_m);
		return false;
	}

	return true;
}

float UBRGA_WeaponFire::GetCooldownDurationSeconds() const
{
	const FBRWeaponRow* Row = GetWeaponRow();
	if (!Row || Row->RPM <= 0.f)
	{
		// The base REFUSES a zero-length cooldown loudly rather than applying one, so returning
		// 0 here is not "no cooldown by accident" — it is a reported refusal.
		return 0.f;
	}
	return 60.f / Row->RPM;
}

// ================================================================================================
// Activation
// ================================================================================================

void UBRGA_WeaponFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base commits cost + cooldown and will have ended us if the commit failed.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bShotResolved = false;

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (IsLocallyControlled())
	{
		FireLocally();
		return;
	}

	// SERVER, for a remote shooter: do not trace anything yet. Wait for the client's claim and
	// validate it. Tracing here and hoping the two agree is how a shooter gets a "server says
	// you missed" bug that nobody can reproduce.
	TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
		.AddUObject(this, &UBRGA_WeaponFire::OnTargetDataReady);
	ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
}

void UBRGA_WeaponFire::FireLocally()
{
	const FBRWeaponRow* Row = GetWeaponRow();
	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	if (!Row || !ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	FVector ViewLocation, ViewDirection;
	if (!GetViewPoint(ViewLocation, ViewDirection))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const FHitResult Hit = TraceShot(*Row, ViewLocation, ViewDirection);

	// Package the claim. SingleTargetHit carries the actor, the impact point and the bone —
	// exactly the three things the server needs to re-derive and to decide headshot.
	FGameplayAbilityTargetData_SingleTargetHit* HitData = new FGameplayAbilityTargetData_SingleTargetHit(Hit);
	FGameplayAbilityTargetDataHandle DataHandle;
	DataHandle.Add(HitData);

	{
		// The prediction window is what lets the cue and the (predicted) effects run now and be
		// reconciled later. Outside it, a predicted GE would never be rolled back.
		FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());

		ASC->ServerSetReplicatedTargetData(
			CurrentSpecHandle,
			CurrentActivationInfo.GetActivationPredictionKey(),
			DataHandle,
			FGameplayTag(),
			ASC->ScopedPredictionKey);
	}

	// Resolve locally too: on the listen-server host this IS the authoritative path, and on a
	// remote client it is the prediction.
	OnTargetDataReady(DataHandle, FGameplayTag());
}

// ================================================================================================
// The gate
// ================================================================================================

void UBRGA_WeaponFire::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag)
{
	if (bShotResolved)
	{
		// A second TargetData packet for one activation must not fire a second shot.
		return;
	}
	bShotResolved = true;

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	const FBRWeaponRow* Row = GetWeaponRow();
	if (!ASC || !Row)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (ASC->GetOwnerRole() == ROLE_Authority)
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* Claim = Data ? Data->GetHitResult() : nullptr;

	// The fire cue is played on BOTH sides of the branch and BEFORE validation, deliberately:
	// the muzzle flash is a consequence of pulling the trigger, not of hitting anything. A cue
	// gated on a server hit makes every miss look like a jam.
	if (Row->FireCueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Claim ? Claim->TraceStart : FVector::ZeroVector;
		CueParams.Instigator = GetAvatarActorFromActorInfo();

		// THE HIT RESULT TRAVELS WITH THE CUE, and this is not optional decoration. A tracer is a
		// beam and a beam needs BOTH ends: `Location` is only the muzzle. Without the hit result
		// the handler has no impact point, and the sole alternative — extrapolating from the
		// shooter's CURRENT aim at draw time — is a convincing lie about where the bullet went,
		// because the shot was fired at the aim of an earlier frame.
		// Found by the cue-library packet, which correctly reports the tracer slot SILENT rather
		// than drawing a plausible one (BP03 Log).
		if (Claim)
		{
			FGameplayEffectContextHandle CueContext = ASC->MakeEffectContext();
			CueContext.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
			CueContext.AddHitResult(*Claim);
			CueParams.EffectContext = CueContext;
		}

		ASC->ExecuteGameplayCue(Row->FireCueTag, CueParams);
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		bool bAccepted = false;
		FString Reason;

		if (!Claim)
		{
			Reason = TEXT("no hit result in the client's target data");
		}
		else if (ValidateClaim(*Claim, *Row, Reason))
		{
			UBRWeaponInstance* Weapon = GetActiveWeapon();

			// Ammo is consumed on the SERVER only, and only for a shot that passed validation.
			// A rejected claim must not cost the player a round — that would let a bad network
			// path drain a magazine.
			if (Weapon && Weapon->ConsumeAmmoForShot())
			{
				bAccepted = true;
				ApplyDamage(*Claim, *Row);
			}
			else
			{
				Reason = TEXT("server had no ammo for this shot");
			}
		}

		if (!bAccepted)
		{
			// REJECTION IS LOUD. A silently dropped shot is indistinguishable from a bug, and
			// this log line is the anti-cheat's only witness until BP03 step 3's cheat tests
			// exist.
			UE_LOG(LogBRCombat, Warning, TEXT("BRGA_WeaponFire REJECTED a client claim: %s"), *Reason);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

bool UBRGA_WeaponFire::ValidateClaim(const FHitResult& Claim, const FBRWeaponRow& Row, FString& OutReason) const
{
	FVector ServerLocation, ServerDirection;
	if (!GetViewPoint(ServerLocation, ServerDirection))
	{
		OutReason = TEXT("server could not resolve the shooter's view point");
		return false;
	}

	// 1. RANGE. The claim must be inside the row's reach, measured from the SERVER's muzzle.
	const float MaxRangeUU = Row.Range_m * BRUnits::MetresToUU + RangeToleranceUU;
	const float ClaimedDistance = FVector::Dist(ServerLocation, Claim.ImpactPoint);
	if (ClaimedDistance > MaxRangeUU)
	{
		OutReason = FString::Printf(
			TEXT("claimed impact at %.0f uu exceeds Range_m %.1f m (+tolerance) = %.0f uu"),
			ClaimedDistance, Row.Range_m, MaxRangeUU);
		return false;
	}

	// 2. CONE. The direction to the claimed impact must lie within Spread_deg of where the
	// server believes the shooter is looking. This is what stops a client claiming a hit behind
	// itself, which no amount of range checking catches.
	const FVector ToImpact = (Claim.ImpactPoint - ServerLocation).GetSafeNormal();
	if (!ToImpact.IsNearlyZero())
	{
		const float CosAngle = FVector::DotProduct(ServerDirection, ToImpact);
		// A zero/absent Spread_deg must not mean "accept nothing" — it means "no cone authored
		// yet", so fall back to a permissive-but-finite bound rather than rejecting every shot.
		const float AcceptHalfAngleDeg = Row.Spread_deg > 0.f ? Row.Spread_deg : 5.f;
		const float MinCos = FMath::Cos(FMath::DegreesToRadians(FMath::Min(AcceptHalfAngleDeg + 2.f, 89.f)));
		if (CosAngle < MinCos)
		{
			OutReason = FString::Printf(
				TEXT("claimed direction is %.1f deg off the server's view; Spread_deg is %.2f"),
				FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.f, 1.f))), Row.Spread_deg);
			return false;
		}
	}

	// 3. RATE is NOT re-checked here. `GE_Cooldown` already blocks a second activation before
	// 60/RPM has elapsed, on the server, through CheckCooldown — re-implementing it here would
	// be a second source of truth for the same rule. BP03 step 3's cheat test drives an
	// over-rate client specifically to prove the cooldown is what stops it.

	return true;
}

void UBRGA_WeaponFire::ApplyDamage(const FHitResult& Hit, const FBRWeaponRow& Row) const
{
	UBRAbilitySystemComponent* SourceASC = GetBRAbilitySystemComponent();
	AActor* HitActor = Hit.GetActor();
	if (!SourceASC || !HitActor)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!TargetASC || TargetASC == SourceASC)
	{
		// No ASC means geometry, not a fighter. Self-hits are dropped rather than reflected.
		return;
	}

	// The damage TYPE and MODIFIER tags. The family is FLAT (R22): {Damage.Kinetic,
	// Damage.Headshot}, never Damage.Kinetic.Headshot.
	FGameplayTagContainer DamageTags;
	DamageTags.AddTag(BRGameplayTags::Damage_Kinetic);
	if (Hit.BoneName == HeadBoneName)
	{
		// The server decides headshot legitimacy from the validated hit, never from a client
		// claim of "this was a headshot" (`gas-purity.md` §5).
		DamageTags.AddTag(BRGameplayTags::Damage_Headshot);
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	Context.AddHitResult(Hit);

	// ONE pipeline. This ability supplies a base number and tags; BRDamageExecCalc reads
	// CT_Combat and BRAttributeSet decides what shields and health do about it.
	const FGameplayEffectSpecHandle Spec = UBRGE_Damage::MakeSpec(SourceASC, Row.DamagePerShot, DamageTags, Context);
	UBRGE_Damage::ApplyToTarget(Spec, SourceASC, TargetASC);
}

// ================================================================================================
// Teardown and helpers
// ================================================================================================

void UBRGA_WeaponFire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Every path tears the delegate down, including the cancelled one. A leaked target-data
	// delegate re-fires against the NEXT activation's prediction key and resolves a shot that
	// was never taken.
	if (TargetDataDelegateHandle.IsValid())
	{
		if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
				.Remove(TargetDataDelegateHandle);
		}
		TargetDataDelegateHandle.Reset();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UBRWeaponInstance* UBRGA_WeaponFire::GetActiveWeapon() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return nullptr;
	}
	const UBREquipmentComponent* Equipment = Avatar->FindComponentByClass<UBREquipmentComponent>();
	return Equipment ? Equipment->GetActiveWeapon() : nullptr;
}

const FBRWeaponRow* UBRGA_WeaponFire::GetWeaponRow() const
{
	const UBRWeaponInstance* Weapon = GetActiveWeapon();
	return Weapon ? Weapon->GetRow() : nullptr;
}

bool UBRGA_WeaponFire::GetViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return false;
	}

	// GetActorEyesViewPoint is the seam that works on BOTH sides: on the server for a remote
	// pawn it returns the replicated control rotation, which is precisely the "server muzzle"
	// the validation is supposed to compare against.
	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(OutLocation, ViewRotation);
	OutDirection = ViewRotation.Vector();
	return true;
}

FHitResult UBRGA_WeaponFire::TraceShot(const FBRWeaponRow& Row, const FVector& From, const FVector& Direction) const
{
	FHitResult Hit;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Hit;
	}

	const FVector To = From + Direction * (Row.Range_m * BRUnits::MetresToUU);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRWeaponFire), /*bTraceComplex=*/true);
	Params.AddIgnoredActor(GetAvatarActorFromActorInfo());
	Params.bReturnPhysicalMaterial = true;

	// bTraceComplex so BoneName comes back populated — headshot detection depends on it.
	//
	// `BRCollision::WeaponTrace`, NOT a raw `ECC_GameTraceChannel*`. This line originally read
	// `ECC_GameTraceChannel1`, which is `BRCollision::Projectile` — the First Person template's
	// own object channel, aliased in BRCore.h precisely so nobody reuses the slot. The failure
	// mode is SILENT: the trace still runs and simply stops hitting the things a weapon should
	// hit, which is exactly what BRCore.h's own warning predicts. Caught by the BP05 melee
	// packet reading this file, not by the author. Use the alias, never the number.
	World->LineTraceSingleByChannel(Hit, From, To, BRCollision::WeaponTrace, Params);

	if (!Hit.bBlockingHit)
	{
		// A miss still carries its geometry, so the server can validate the direction of a shot
		// that hit nothing and the cue still knows where the tracer went.
		Hit.TraceStart = From;
		Hit.TraceEnd = To;
		Hit.ImpactPoint = To;
	}
	return Hit;
}
