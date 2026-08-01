// Breachpoint. The CMC subclass: sprint intent, the saved move that replays it, the speed rule.

#include "Character/BRCharacterMovementComponent.h"

#include "GameFramework/Character.h"

#include "AbilitySystem/BRCombatCurves.h"
#include "Core/BRCore.h"

// ===========================================================================================
// FSavedMove_BR — the four hooks, none optional
// ===========================================================================================

void FSavedMove_BR::Clear()
{
	Super::Clear();

	// THE CLASSIC BUG this line prevents: moves are pooled and reused, so a sprint flag left over
	// from three moves ago replays as a phantom input the player never gave.
	bSavedWantsToSprint = 0;
}

void FSavedMove_BR::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UBRCharacterMovementComponent* Movement = C ? Cast<UBRCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		bSavedWantsToSprint = Movement->bWantsToSprint;
	}
}

void FSavedMove_BR::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UBRCharacterMovementComponent* Movement = C ? Cast<UBRCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		// The replay path. The component is rewound to the intent this move was RECORDED with, not
		// the intent the player has now — which is the whole point of a saved move.
		Movement->bWantsToSprint = bSavedWantsToSprint;
	}
}

bool FSavedMove_BR::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_BR* Other = static_cast<const FSavedMove_BR*>(NewMove.Get());
	if (Other && Other->bSavedWantsToSprint != bSavedWantsToSprint)
	{
		// Combining moves that disagree about intent is how the frame you started sprinting
		// silently disappears on a correction. Refuse before asking the base anything else.
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

	return Result;
}

FSavedMovePtr FNetworkPredictionData_Client_BR::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_BR());
}

// ===========================================================================================
// UBRCharacterMovementComponent
// ===========================================================================================

UBRCharacterMovementComponent::UBRCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bWantsToSprint(0)
{
	// Halo-feel movement numbers (walk/air speed, air control, jump velocity, gravity scale) are
	// §3.4 "config on CMC defaults, not code" — this constructor still sets NONE of them, and the
	// sprint multiplier is a CT_Combat curve rather than a member. A number typed here would be a
	// law-3 violation waiting to be found.
}

void UBRCharacterMovementComponent::SetSprintIntent(bool bNewWantsToSprint)
{
	const uint8 NewValue = bNewWantsToSprint ? 1 : 0;
	if (bWantsToSprint == NewValue)
	{
		return;
	}

	bWantsToSprint = NewValue;

	UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacterMovementComponent '%s': sprint intent -> %s."),
		*GetNameSafe(GetOwner()), bNewWantsToSprint ? TEXT("ON") : TEXT("OFF"));
}

bool UBRCharacterMovementComponent::IsSprintIntentValid() const
{
	// Airborne sprint would let a bunny-hopper carry sprint speed through the air, which is a
	// movement design nobody asked for. IsMovingOnGround() covers MOVE_Walking and MOVE_NavWalking
	// (bots) in one call. This is a rule, not a tuning value, so it lives in code.
	return bWantsToSprint && IsMovingOnGround();
}

float UBRCharacterMovementComponent::GetSprintSpeedMultiplier() const
{
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

	// 1.0 is the IDENTITY, not a guess at a sprint speed: with no curve the player walks while
	// holding sprint, which is visible in the first ten seconds of a playtest. Substituting a
	// plausible 1.4 here would hide a missing CSV row behind a number nobody chose.
	UE_LOG(LogBRCombat, Error, TEXT("BRCharacterMovementComponent: CT_Combat has no usable '%s' curve. Sprint applies NO speed multiplier."),
		*BRCombatCurves::Names::MovementSprintSpeedMultiplier.ToString());

	CachedSprintSpeedMultiplier = 1.f;
	return CachedSprintSpeedMultiplier;
}

float UBRCharacterMovementComponent::GetMaxSpeed() const
{
	const float BaseMaxSpeed = Super::GetMaxSpeed();

	if (!IsSprintIntentValid())
	{
		return BaseMaxSpeed;
	}

	// Multiplying the engine's answer (rather than returning a sprint speed) means crouch, water
	// and every future movement mode keep their own ceilings and sprint scales whatever is current.
	return BaseMaxSpeed * GetSprintSpeedMultiplier();
}

void UBRCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// Symmetric with FSavedMove_BR::GetCompressedFlags. Asymmetry here is the bug that makes a
	// client sprint and a server not, and it is invisible in single-process PIE.
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0 ? 1 : 0;
}

FNetworkPredictionData_Client* UBRCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		// The ONE reason this override exists: hand out FSavedMove_BR instead of
		// FSavedMove_Character. Everything else (smoothing distances, timestamps) is initialised by
		// FNetworkPredictionData_Client_Character's constructor and is deliberately not restated
		// here — a "restated engine default" is a second source of truth that silently diverges the
		// next time Epic changes theirs.
		UBRCharacterMovementComponent* MutableThis = const_cast<UBRCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_BR(*this);
	}

	return ClientPredictionData;
}
