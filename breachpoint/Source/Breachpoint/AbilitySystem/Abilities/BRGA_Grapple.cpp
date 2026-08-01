// BREACHPOINT — BP06. The Grappleshot: the netcode packet.

#include "AbilitySystem/Abilities/BRGA_Grapple.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "Character/BRCharacterMovementComponent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Weapons/BRWeaponPickup.h"

namespace
{
	/** Metres -> Unreal units. Structural; nobody balances this number. */
	constexpr float MetresToUU = 100.f;

	/**
	 * CT_Combat curve names this ability needs. **NEITHER ROW EXISTS TODAY** — `CT_Combat.csv`
	 * has 11 rows and none of them is a grapple row (verified 1 Aug 2026).
	 *
	 * They are spelled here rather than in `BRCombatCurves::Names` because that header lives in
	 * `AbilitySystem/` and this packet's `owner_path` is `AbilitySystem/Abilities/` — moving
	 * them there is a one-line change for whoever owns BP02, and it should happen. Filed, not
	 * smuggled.
	 *
	 * A NAME is not a NUMBER. Nothing in this file decides what a grapple's range or cooldown
	 * IS; it decides where to read them from. `Evaluate` returns false when the row is missing
	 * and every caller below refuses loudly rather than substituting a default — a grapple that
	 * silently used a hardcoded 20 m would be a law-3 violation wearing a working feature's
	 * costume.
	 */
	const FName GrappleRangeMetresCurve(TEXT("Grapple.RangeMetres"));
	const FName GrappleCooldownSecondsCurve(TEXT("Grapple.CooldownSeconds"));
}

UBRGA_Grapple::UBRGA_Grapple(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// LocalPredicted is not a preference here, it is the feature. An unpredicted grapple is a
	// grapple that starts 120 ms after the button, and `cmc-prediction` §1 is explicit that the
	// payoff of riding the saved-move pipeline is the difference between instant and rubber-band.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Grapple);
	SetAssetTags(AssetTags);

	// The ability's own tag doubles as its cooldown tag — the generic-cooldown pattern the base
	// implements. This is what makes the cooldown a PREDICTED GE, and therefore what makes it
	// roll back for free when the server rejects the grapple. It is one of the three legs of
	// "rejection leaves zero state" and the only one that is a single line.
	CooldownTag = BRGameplayTags::Ability_Grapple;

	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	bCommitOnActivate = true;
}

float UBRGA_Grapple::GetCooldownDurationSeconds() const
{
	float Seconds = 0.f;
	if (BRCombatCurves::Evaluate(GrappleCooldownSecondsCurve, Seconds) && Seconds > 0.f)
	{
		return Seconds;
	}

	// The base REFUSES LOUDLY when an ability declares a cooldown tag and no duration, rather
	// than applying a zero-length cooldown. That is the behaviour we want: a 20 s cooldown that
	// silently became 0 is the rocket-cooldown bug, and BP06's whole point is that a rejected
	// grapple must not consume a cooldown — which is meaningless if there is no cooldown.
	UE_LOG(LogBRCombat, Warning,
		TEXT("UBRGA_Grapple: CT_Combat has no '%s' row. The cooldown cannot be authored, and the "
			 "base will refuse rather than apply a zero-length one. Row owed by BP13/curator."),
		*GrappleCooldownSecondsCurve.ToString());
	return 0.f;
}

float UBRGA_Grapple::GetMaxRangeMetres() const
{
	float Metres = 0.f;
	if (BRCombatCurves::Evaluate(GrappleRangeMetresCurve, Metres) && Metres > 0.f)
	{
		return Metres;
	}
	return 0.f;
}

// ================================================================================================
// Activation
// ================================================================================================

void UBRGA_Grapple::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bResolved = false;
	ActiveMode = EBRGrappleMode::None;

	const float RangeMetres = GetMaxRangeMetres();
	if (RangeMetres <= 0.f)
	{
		// No authored range means no trace length. Refusing is the only lawful answer — see the
		// note on the curve names above. The fire path took the same position about Range_m.
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Grapple refused: CT_Combat has no '%s' row, so there is no range to trace. "
				 "A hardcoded 20 m here would be a law-3 violation."),
			*GrappleRangeMetresCurve.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FHitResult Hit;
	if (!TraceForTarget(Hit))
	{
		// A whiff. **Cancelled, not completed** — bWasCancelled=true is what tells GAS to roll
		// back the predicted cooldown, and "a whiff does not consume the cooldown" is in BP06's
		// Done-when.
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const EBRGrappleMode Mode = ClassifyHit(Hit);
	if (Mode == EBRGrappleMode::None)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		FString Reason;
		if (!ValidateTarget(Hit, Mode, Reason))
		{
			UE_LOG(LogBRCombat, Warning, TEXT("UBRGA_Grapple REJECTED: %s"), *Reason);
			// Cancel, so the client's predicted cooldown and RMS both roll back. The server
			// never applied an RMS, so there is no position to unwind on this side.
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	BeginPull(Hit, Mode);
}

bool UBRGA_Grapple::TraceForTarget(FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!World || !Avatar)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	const FVector End = ViewLocation + ViewRotation.Vector() * (GetMaxRangeMetres() * MetresToUU);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRGrapple), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Avatar);

	// BRCollision::GrappleTrace, never a raw channel number. See the note in BRGA_WeaponFire:
	// the raw enum silently traces on the template's Projectile channel and simply stops
	// hitting things. BRCore.h aliases all four channels so this cannot happen by typo.
	return World->LineTraceSingleByChannel(OutHit, ViewLocation, End, BRCollision::GrappleTrace, Params)
		&& OutHit.bBlockingHit;
}

EBRGrappleMode UBRGA_Grapple::ClassifyHit(const FHitResult& Hit) const
{
	AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		// Hit the world with no actor — still geometry, still a valid self-pull anchor.
		return EBRGrappleMode::SelfPull;
	}

	if (HitActor->IsA<ABRWeaponPickup>())
	{
		return EBRGrappleMode::WeaponAttract;
	}

	if (HitActor->IsA<APawn>())
	{
		return EBRGrappleMode::PawnReel;
	}

	return EBRGrappleMode::SelfPull;
}

bool UBRGA_Grapple::ValidateTarget(const FHitResult& Claim, EBRGrappleMode ClaimedMode, FString& OutReason) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = GetWorld();
	if (!Avatar || !World)
	{
		OutReason = TEXT("server could not resolve the avatar or world");
		return false;
	}

	FVector ServerLocation;
	FRotator ServerRotation;
	Avatar->GetActorEyesViewPoint(ServerLocation, ServerRotation);

	// 1. RANGE, from the SERVER's eyes.
	const float MaxUU = GetMaxRangeMetres() * MetresToUU;
	const float Distance = FVector::Dist(ServerLocation, Claim.ImpactPoint);
	if (Distance > MaxUU)
	{
		OutReason = FString::Printf(TEXT("target at %.0f uu exceeds grapple range %.0f uu"), Distance, MaxUU);
		return false;
	}

	// 2. LINE OF SIGHT, re-derived rather than trusted. This is what stops a grapple THROUGH
	// geometry — step 5's critic case names it explicitly.
	FHitResult LosHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRGrappleLOS), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Avatar);
	if (Claim.GetActor())
	{
		Params.AddIgnoredActor(Claim.GetActor());
	}
	if (World->LineTraceSingleByChannel(LosHit, ServerLocation, Claim.ImpactPoint, BRCollision::GrappleTrace, Params)
		&& LosHit.bBlockingHit)
	{
		OutReason = FString::Printf(TEXT("no line of sight — '%s' blocks the path to the target"),
			*GetNameSafe(LosHit.GetActor()));
		return false;
	}

	// 3. MODE AGREEMENT. The server classifies the hit itself; if its answer differs from the
	// client's, the shapes disagree and the safe move is to refuse. Classification reads only
	// the actor's TYPE precisely so this comparison is meaningful.
	if (ClassifyHit(Claim) != ClaimedMode)
	{
		OutReason = TEXT("server classified the target differently from the client");
		return false;
	}

	// 4. RATE is not re-checked here. `GE_Cooldown` enforces it server-side through
	// CheckCooldown; a second check would be a second source of truth for one rule.
	return true;
}

// ================================================================================================
// Motion — asked for, never performed here
// ================================================================================================

void UBRGA_Grapple::BeginPull(const FHitResult& Hit, EBRGrappleMode Mode)
{
	if (bResolved)
	{
		return;
	}
	bResolved = true;
	ActiveMode = Mode;

	switch (Mode)
	{
	case EBRGrappleMode::SelfPull:
	case EBRGrappleMode::PawnReel:
	{
		// ONE-BODY MOTION. **Decision required by BP06 step 2, taken here and logged:** a pawn
		// reel pulls the SHOOTER to the target, and the target is unaffected. Two-body pull is
		// Phase 2.
		//
		// The reason is netcode, not scope: moving a pawn you do not own means either an
		// unpredicted server push (which snaps on the victim's screen, and the victim is the one
		// player guaranteed to notice) or two-body prediction, where both clients must agree
		// about a shared correction. One-body reel is predicted by the SAME root-motion path as
		// a self-pull — the mode changes what we aim at, not how motion works. One mechanism,
		// three modes.
		if (UBRCharacterMovementComponent* Movement = GetBRMovement())
		{
			Movement->StartGrapplePull(Hit.ImpactPoint);
		}
		else
		{
			UE_LOG(LogBRCombat, Error,
				TEXT("UBRGA_Grapple: no UBRCharacterMovementComponent — the pull cannot be "
					 "predicted and will not be faked with a position write."));
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
		break;
	}

	case EBRGrappleMode::WeaponAttract:
	{
		// The shooter does not move; the pickup travels. The pickup owns its own replicated
		// motion — this ability only names the destination, on the authority.
		if (HasAuthority(&CurrentActivationInfo))
		{
			if (ABRWeaponPickup* Pickup = Cast<ABRWeaponPickup>(Hit.GetActor()))
			{
				// SEAM OWED (BP06 step 2): ABRWeaponPickup has no attract entry point yet. Filed
				// rather than worked around — writing the pickup's motion from here would put a
				// second owner on an actor that already replicates its own state, which is the
				// bug law 7 exists to prevent.
				UE_LOG(LogBRCombat, Warning,
					TEXT("UBRGA_Grapple: WeaponAttract has no seam on '%s' yet. "
						 "ABRWeaponPickup owes an authority-side attract entry point; this "
						 "ability must not move it from here."),
					*GetNameSafe(Pickup));
			}
		}
		break;
	}

	default:
		break;
	}

	// The rope cue is raised inside the prediction window so GAS removes it on rollback. A
	// particle spawned from ability body code would stay on screen forever after a rejection —
	// that is the third leg of "rejection leaves zero state".
	//
	// CUE TAG OWED: `GameplayCue.Grapple.Rope` is not declared. `GameplayCue.*` is an OPEN
	// family (R23) and this packet may declare it — but the CUE HANDLER is a C++ class (law 7,
	// R18) that does not exist either, so declaring the tag alone would produce a cue that
	// silently never plays. Filed as one unit of work rather than half-done.
}

void UBRGA_Grapple::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Tear down exactly what was started, on EVERY path including cancellation. The CMC removes
	// its own source by the handle it retained — this file does not clear root motion wholesale,
	// because a blanket clear would also kill an unrelated knockback later.
	if (ActiveMode == EBRGrappleMode::SelfPull || ActiveMode == EBRGrappleMode::PawnReel)
	{
		if (UBRCharacterMovementComponent* Movement = GetBRMovement())
		{
			Movement->StopGrapplePull();
		}
	}
	ActiveMode = EBRGrappleMode::None;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UBRCharacterMovementComponent* UBRGA_Grapple::GetBRMovement() const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	return Character ? Cast<UBRCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}
