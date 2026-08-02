#include "Character/BRCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

#include "AbilitySystem/BRAttributeSet.h"

#include "GameFramework/Character.h"
#include "GameFramework/RootMotionSource.h"

#include "AbilitySystem/BRCombatCurves.h"
#include "Core/BRCore.h"

namespace
{
	namespace BRGrappleCurveNames
	{
		const FName GrapplePullSpeed = FName(TEXT("Movement.Grapple.PullSpeed"));

		const FName GrappleArrivalRadius = FName(TEXT("Movement.Grapple.ArrivalRadius"));

		const FName GrappleMaxPullSeconds = FName(TEXT("Movement.Grapple.MaxPullSeconds"));
	}

	const FName GrapplePullSourceName = FName(TEXT("BRGrapplePull"));
	constexpr uint16 GrapplePullSourcePriority = 5;
}

void FSavedMove_BR::Clear()
{
	Super::Clear();

	bSavedWantsToSprint = 0;
	bSavedWantsToGrapple = 0;
}

void FSavedMove_BR::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UBRCharacterMovementComponent* Movement = C ? Cast<UBRCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		bSavedWantsToSprint = Movement->bWantsToSprint;
		bSavedWantsToGrapple = Movement->bWantsToGrapple;
	}
}

void FSavedMove_BR::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UBRCharacterMovementComponent* Movement = C ? Cast<UBRCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		Movement->bWantsToSprint = bSavedWantsToSprint;
		Movement->bWantsToGrapple = bSavedWantsToGrapple;
	}
}

bool FSavedMove_BR::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_BR* Other = static_cast<const FSavedMove_BR*>(NewMove.Get());
	if (Other && Other->bSavedWantsToSprint != bSavedWantsToSprint)
	{
		return false;
	}

	if (Other && Other->bSavedWantsToGrapple != bSavedWantsToGrapple)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

uint8 FSavedMove_BR::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bSavedWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}

	if (bSavedWantsToGrapple)
	{
		Result |= FLAG_Custom_1;
	}

	return Result;
}

FSavedMovePtr FNetworkPredictionData_Client_BR::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_BR());
}

UBRCharacterMovementComponent::UBRCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bWantsToSprint(0)
	, bWantsToGrapple(0)
	, bGrappleIntentObserved(0)
{
}

void UBRCharacterMovementComponent::SetSprintIntent(bool bNewWantsToSprint)
{
	const uint8 NewValue = bNewWantsToSprint ? 1 : 0;
	if (bWantsToSprint == NewValue)
	{
		return;
	}

	bWantsToSprint = NewValue;
}

bool UBRCharacterMovementComponent::IsSprintIntentValid() const
{
	return bWantsToSprint && IsMovingOnGround();
}

float UBRCharacterMovementComponent::GetSprintSpeedMultiplier() const
{
	// ATTRIBUTE FIRST, curve second. The attribute is what a GameplayEffect can buff; the curve
	// is CT_Combat.csv's authored default and the floor this falls back to when no ASC has
	// replicated yet - a joining client, or a bot with no attribute set. Zero means unset, not
	// "stand still".
	if (const UBRAttributeSet* Attributes = GetBRAttributeSet())
	{
		const float FromAttribute = Attributes->GetSprintSpeedMultiplier();
		if (FromAttribute > KINDA_SMALL_NUMBER)
		{
			return FromAttribute;
		}
	}

	if (CachedSprintSpeedMultiplier >= 0.f)
	{
		return CachedSprintSpeedMultiplier;
	}

	float Multiplier = 0.f;
	if (BRCombatCurves::Evaluate(BRCombatCurves::Names::MovementSprintSpeedMultiplier, Multiplier) && Multiplier > 0.f)
	{
		CachedSprintSpeedMultiplier = Multiplier;
		return CachedSprintSpeedMultiplier;
	}

	// The C++ default, not 1.0. Returning 1.0 here means "sprint does nothing" while every log
	// reports sprint active - a silent no-op is worse than a wrong number.
	CachedSprintSpeedMultiplier = BRAttributeDefaults::SprintSpeedMultiplier;
	return CachedSprintSpeedMultiplier;
}

const UBRAttributeSet* UBRCharacterMovementComponent::GetBRAttributeSet() const
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	return ASC ? ASC->GetSet<UBRAttributeSet>() : nullptr;
}

float UBRCharacterMovementComponent::GetMaxSpeed() const
{
	// Super, not the attribute, supplies the base: it already resolves crouch, swim and fly
	// modes, and an attribute override would silently discard MaxWalkSpeedCrouched.
	// MoveSpeedBase overrides ONLY the walking case, and only when a GE has actually set it.
	float BaseMaxSpeed = Super::GetMaxSpeed();

	if (IsMovingOnGround() && !IsCrouching())
	{
		if (const UBRAttributeSet* Attributes = GetBRAttributeSet())
		{
			const float FromAttribute = Attributes->GetMoveSpeedBase();
			if (FromAttribute > KINDA_SMALL_NUMBER)
			{
				BaseMaxSpeed = FromAttribute;
			}
		}
	}

	// THE BRANCH IS THE PREDICTED PART. bWantsToSprint arrives through UpdateFromCompressedFlags
	// and is carried in FSavedMove_BR, so a correction replays the same on/off decision it made
	// originally. Only the MAGNITUDE comes from an attribute, and a magnitude changes when a buff
	// lands - rarely - where the on/off changes several times a second.
	if (!IsSprintIntentValid())
	{
		return BaseMaxSpeed;
	}

	return BaseMaxSpeed * GetSprintSpeedMultiplier();
}

void UBRCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0 ? 1 : 0;
	bWantsToGrapple = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0 ? 1 : 0;

	if (bWantsToGrapple)
	{
		bGrappleIntentObserved = 1;
	}
}

FNetworkPredictionData_Client* UBRCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UBRCharacterMovementComponent* MutableThis = const_cast<UBRCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_BR(*this);
	}

	return ClientPredictionData;
}

const FBRGrappleTuning& UBRCharacterMovementComponent::GetGrappleTuning() const
{
	if (bGrappleTuningResolved)
	{
		return CachedGrappleTuning;
	}

	bGrappleTuningResolved = true;

	float PullSpeed = 0.f;
	float ArrivalRadius = 0.f;
	float MaxSeconds = 0.f;

	const bool bHavePullSpeed = BRCombatCurves::Evaluate(BRGrappleCurveNames::GrapplePullSpeed, PullSpeed) && PullSpeed > 0.f;
	const bool bHaveArrival = BRCombatCurves::Evaluate(BRGrappleCurveNames::GrappleArrivalRadius, ArrivalRadius) && ArrivalRadius > 0.f;
	const bool bHaveMaxSeconds = BRCombatCurves::Evaluate(BRGrappleCurveNames::GrappleMaxPullSeconds, MaxSeconds) && MaxSeconds > 0.f;

	if (bHavePullSpeed && bHaveArrival && bHaveMaxSeconds)
	{
		CachedGrappleTuning.PullSpeedCmPerSecond = PullSpeed;
		CachedGrappleTuning.ArrivalRadiusCm = ArrivalRadius;
		CachedGrappleTuning.MaxPullSeconds = MaxSeconds;
		CachedGrappleTuning.bValid = true;
		return CachedGrappleTuning;
	}

	CachedGrappleTuning = FBRGrappleTuning();
	return CachedGrappleTuning;
}

bool UBRCharacterMovementComponent::IsGrapplePullActive()
{
	if (ActiveGrapplePullSourceID == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		return false;
	}

	return CurrentRootMotion.GetRootMotionSourceByID(ActiveGrapplePullSourceID).IsValid();
}

bool UBRCharacterMovementComponent::StartGrapplePull(const FVector& PullTargetWorld)
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return false;
	}

	const ENetRole LocalRole = CharacterOwner->GetLocalRole();
	if (LocalRole != ROLE_Authority && LocalRole != ROLE_AutonomousProxy)
	{
		return false;
	}

	const FBRGrappleTuning& Tuning = GetGrappleTuning();
	if (!Tuning.bValid)
	{
		return false;
	}

	StopGrapplePull();

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const float Distance = FVector::Dist(Start, PullTargetWorld);
	if (Distance <= Tuning.ArrivalRadiusCm)
	{
		return false;
	}

	const float Duration = FMath::Min(Distance / Tuning.PullSpeedCmPerSecond, Tuning.MaxPullSeconds);

	TSharedPtr<FRootMotionSource_MoveToForce> Pull = MakeShared<FRootMotionSource_MoveToForce>();
	Pull->InstanceName = GrapplePullSourceName;
	Pull->Priority = GrapplePullSourcePriority;
	Pull->AccumulateMode = ERootMotionAccumulateMode::Override;
	Pull->StartLocation = Start;
	Pull->TargetLocation = PullTargetWorld;
	Pull->Duration = Duration;

	Pull->bRestrictSpeedToExpected = false;

	Pull->PathOffsetCurve = nullptr;

	if (IsMovingOnGround())
	{
		SetMovementMode(MOVE_Falling);
	}

	ActiveGrapplePullSourceID = ApplyRootMotionSource(Pull);
	if (ActiveGrapplePullSourceID == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		return false;
	}

	GrapplePullTarget = PullTargetWorld;
	bWantsToGrapple = 1;

	bGrappleIntentObserved = 0;

	return true;
}

void UBRCharacterMovementComponent::StopGrapplePull()
{
	if (ActiveGrapplePullSourceID != static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		RemoveRootMotionSourceByID(ActiveGrapplePullSourceID);
	}

	ClearGrapplePullState();
}

void UBRCharacterMovementComponent::ClearGrapplePullState()
{
	const bool bWasSomething = ActiveGrapplePullSourceID != static_cast<uint16>(ERootMotionSourceID::Invalid) || bWantsToGrapple;

	ActiveGrapplePullSourceID = static_cast<uint16>(ERootMotionSourceID::Invalid);
	GrapplePullTarget = FVector::ZeroVector;
	bWantsToGrapple = 0;
	bGrappleIntentObserved = 0;

}

void UBRCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (CharacterOwner && CharacterOwner->bPressedJump && IsGrapplePullActive())
	{
		StopGrapplePull();
	}

	if (!bWantsToGrapple && bGrappleIntentObserved && IsGrapplePullActive())
	{
		StopGrapplePull();
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UBRCharacterMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);

	if (ActiveGrapplePullSourceID == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		return;
	}

	if (!IsGrapplePullActive())
	{
		ClearGrapplePullState();
		return;
	}

	const FBRGrappleTuning& Tuning = GetGrappleTuning();
	if (Tuning.bValid && UpdatedComponent)
	{
		const float DistanceToTarget = FVector::Dist(UpdatedComponent->GetComponentLocation(), GrapplePullTarget);
		if (DistanceToTarget <= Tuning.ArrivalRadiusCm)
		{
			StopGrapplePull();
		}
	}
}
