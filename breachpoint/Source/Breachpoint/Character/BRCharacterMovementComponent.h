#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "BRCharacterMovementComponent.generated.h"

struct FSavedMove_BR : public FSavedMove_Character
{
	using Super = FSavedMove_Character;

	uint8 bSavedWantsToSprint : 1;

	uint8 bSavedWantsToGrapple : 1;

	FSavedMove_BR()
		: bSavedWantsToSprint(0)
		, bSavedWantsToGrapple(0)
	{
	}

	virtual void Clear() override;

	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;

	virtual void PrepMoveFor(ACharacter* C) override;

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;

	virtual uint8 GetCompressedFlags() const override;
};

class FNetworkPredictionData_Client_BR : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_BR(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override;
};

struct FBRGrappleTuning
{
	float PullSpeedCmPerSecond = 0.f;

	float ArrivalRadiusCm = 0.f;

	float MaxPullSeconds = 0.f;

	bool bValid = false;
};

UCLASS(meta = (DisplayName = "BR Character Movement Component"))
class BREACHPOINT_API UBRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UBRCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	void SetSprintIntent(bool bNewWantsToSprint);

	bool WantsToSprint() const { return bWantsToSprint; }

	bool StartGrapplePull(const FVector& PullTargetWorld);

	void StopGrapplePull();

	bool WantsToGrapple() const { return bWantsToGrapple; }

	bool IsGrapplePullActive();

	virtual bool IsSprintIntentValid() const;

	virtual float GetMaxSpeed() const override;

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;

protected:
	uint8 bWantsToSprint : 1;

	uint8 bWantsToGrapple : 1;

private:
	float GetSprintSpeedMultiplier() const;

	// The ASC lives on the PlayerState, so this resolves through the owner rather than caching -
	// a cache would go stale on every respawn.
	const class UBRAttributeSet* GetBRAttributeSet() const;

	mutable float CachedSprintSpeedMultiplier = -1.f;

	const FBRGrappleTuning& GetGrappleTuning() const;

	mutable FBRGrappleTuning CachedGrappleTuning;
	mutable bool bGrappleTuningResolved = false;

	uint16 ActiveGrapplePullSourceID = 0;

	FVector GrapplePullTarget = FVector::ZeroVector;

	uint8 bGrappleIntentObserved : 1;

	void ClearGrapplePullState();

	friend struct FSavedMove_BR;
};
