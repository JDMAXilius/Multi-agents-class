#include "Characters/BNCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/RootMotionSource.h"

namespace
{
	const FName GrapplePullSourceName = FName(TEXT("BNGrapplePull"));
	// Above the engine's default sources so the pull wins a tug-of-war with ordinary
	// movement, transcribed verbatim (the BR original's number).
	constexpr uint16 GrapplePullSourcePriority = 5;
}

void FSavedMove_BN::Clear()
{
	Super::Clear();

	// Moves are POOLED and reused — a stale flag from three moves ago replays as a
	// phantom grapple (the cmc-prediction skill's classic bug, named before it bites).
	bSavedWantsToGrapple = 0;
}

void FSavedMove_BN::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UBNCharacterMovementComponent* Movement = C ? Cast<UBNCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		bSavedWantsToGrapple = Movement->bWantsToGrapple;
	}
}

void FSavedMove_BN::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UBNCharacterMovementComponent* Movement = C ? Cast<UBNCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		Movement->bWantsToGrapple = bSavedWantsToGrapple;
	}
}

bool FSavedMove_BN::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	// Combining moves that disagree is how a grapple frame silently disappears on
	// correction — differing flags refuse the merge.
	const FSavedMove_BN* Other = static_cast<const FSavedMove_BN*>(NewMove.Get());
	if (Other && Other->bSavedWantsToGrapple != bSavedWantsToGrapple)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

uint8 FSavedMove_BN::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bSavedWantsToGrapple)
	{
		Result |= FLAG_Custom_0;
	}

	return Result;
}

FSavedMovePtr FNetworkPredictionData_Client_BN::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_BN());
}

UBNCharacterMovementComponent::UBNCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bWantsToGrapple(0)
	, bGrappleIntentObserved(0)
{
}

void UBNCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToGrapple = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0 ? 1 : 0;

	if (bWantsToGrapple)
	{
		bGrappleIntentObserved = 1;
	}
}

FNetworkPredictionData_Client* UBNCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UBNCharacterMovementComponent* MutableThis = const_cast<UBNCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_BN(*this);
	}

	return ClientPredictionData;
}

bool UBNCharacterMovementComponent::IsGrapplePullActive()
{
	if (ActiveGrapplePullSourceID == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		return false;
	}

	return CurrentRootMotion.GetRootMotionSourceByID(ActiveGrapplePullSourceID).IsValid();
}

bool UBNCharacterMovementComponent::StartGrapplePull(const FVector& PullTargetWorld)
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return false;
	}

	// Authority applies for real; the autonomous proxy applies its predicted copy.
	// A simulated proxy applies nothing — it watches replication like it watches walking.
	const ENetRole LocalRole = CharacterOwner->GetLocalRole();
	if (LocalRole != ROLE_Authority && LocalRole != ROLE_AutonomousProxy)
	{
		return false;
	}

	if (GrapplePullSpeedUU <= 0.f || GrappleArrivalRadiusUU <= 0.f || GrappleMaxPullSeconds <= 0.f)
	{
		return false; // a bad ini must refuse, not divide
	}

	StopGrapplePull();

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const float Distance = FVector::Dist(Start, PullTargetWorld);
	if (Distance <= GrappleArrivalRadiusUU)
	{
		return false;
	}

	const float Duration = FMath::Min(Distance / GrapplePullSpeedUU, GrappleMaxPullSeconds);

	TSharedPtr<FRootMotionSource_MoveToForce> Pull = MakeShared<FRootMotionSource_MoveToForce>();
	Pull->InstanceName = GrapplePullSourceName;
	Pull->Priority = GrapplePullSourcePriority;
	Pull->AccumulateMode = ERootMotionAccumulateMode::Override;
	Pull->StartLocation = Start;
	Pull->TargetLocation = PullTargetWorld;
	Pull->Duration = Duration;
	Pull->bRestrictSpeedToExpected = false;
	Pull->PathOffsetCurve = nullptr;

	// Off the ground first: a grounded MoveToForce fights floor snapping every frame.
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

void UBNCharacterMovementComponent::StopGrapplePull()
{
	if (ActiveGrapplePullSourceID != static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		RemoveRootMotionSourceByID(ActiveGrapplePullSourceID);
	}

	ClearGrapplePullState();
}

void UBNCharacterMovementComponent::ClearGrapplePullState()
{
	ActiveGrapplePullSourceID = static_cast<uint16>(ERootMotionSourceID::Invalid);
	GrapplePullTarget = FVector::ZeroVector;
	bWantsToGrapple = 0;
	bGrappleIntentObserved = 0;
}

void UBNCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// JUMP CANCELS — the Halo momentum move: a jump pressed mid-pull keeps the pull's
	// velocity and drops the rope.
	if (CharacterOwner && CharacterOwner->bPressedJump && IsGrapplePullActive())
	{
		StopGrapplePull();
	}

	// A replay whose flags dropped the intent after it was observed stops the pull —
	// this is what makes a server rejection actually undo a predicted grapple.
	if (!bWantsToGrapple && bGrappleIntentObserved && IsGrapplePullActive())
	{
		StopGrapplePull();
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UBNCharacterMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);

	if (ActiveGrapplePullSourceID == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		return;
	}

	if (!IsGrapplePullActive())
	{
		// The engine expired the source (Duration lapsed) — clear our book so the next
		// pull starts clean.
		ClearGrapplePullState();
		return;
	}

	if (UpdatedComponent)
	{
		const float DistanceToTarget = FVector::Dist(UpdatedComponent->GetComponentLocation(), GrapplePullTarget);
		if (DistanceToTarget <= GrappleArrivalRadiusUU)
		{
			StopGrapplePull();
		}
	}
}
