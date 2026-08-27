#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "BNCharacterMovementComponent.generated.h"

/**
 * BN23 — BN's first custom CMC, and it exists for exactly ONE thing: the Grappleshot's
 * predicted pull. Everything else BN moves (sprint, ADS slow, knockback) stays GE-driven
 * on purpose — this class is TRANSCRIBED from the legacy module's compiled
 * UBRCharacterMovementComponent (Character/BRCharacterMovementComponent.h/.cpp), the one
 * saved-move implementation in this repo that has been through a compiler, minus its
 * sprint half (BN's sprint is UBNGE_Sprint's multiplier and owes this file nothing).
 *
 * THE BIT BUDGET (the cmc-prediction skill's law): FSavedMove_Character exposes FOUR
 * custom bits. BN spends FLAG_Custom_0 on the grapple; 1-3 are free, and whoever spends
 * the third opens the compressed-flags-vs-move-data ruling the skill defers.
 */
struct FSavedMove_BN : public FSavedMove_Character
{
	using Super = FSavedMove_Character;

	uint8 bSavedWantsToGrapple : 1;

	FSavedMove_BN()
		: bSavedWantsToGrapple(0)
	{
	}

	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual uint8 GetCompressedFlags() const override;
};

class FNetworkPredictionData_Client_BN : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_BN(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override;
};

UCLASS(Config = Game, meta = (DisplayName = "BN Character Movement Component"))
class BREACHPOINTNEXT_API UBNCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UBNCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/** Begin the pull as a root motion source (FRootMotionSource_MoveToForce). Called by
	 *  UBNGA_Grapple on the authority AND the autonomous proxy — the RMS rides the
	 *  saved-move pipeline, which is the whole reason this predicts without bespoke
	 *  reconciliation. False = refused (no tuning, already at the target, bad state). */
	bool StartGrapplePull(const FVector& PullTargetWorld);

	/** Remove exactly OUR source by the retained ID — never a blanket clear, which would
	 *  also kill an unrelated knockback later. */
	void StopGrapplePull();

	bool WantsToGrapple() const { return bWantsToGrapple; }
	bool IsGrapplePullActive();

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	/** Detach rules, both transcribed: jump CANCELS the pull (the Halo momentum move),
	 *  and a dropped intent flag after one was observed stops a replayed pull. */
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	/** Arrival and natural expiry: inside the arrival radius the pull ends; a source the
	 *  engine already expired has its state cleared so the next pull starts clean. */
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;

protected:
	// -- tuning. Config, not a curve table: BN's numbers idiom (the BR original read
	// -- CT_Combat rows; BN has no combat-curve table and the ini is law 7's preference).
	/** How fast the pull travels. 1800uu/s crosses the arena's 400uu tier rise in under
	 *  a quarter second of vertical — a yank, not an elevator. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grapple")
	float GrapplePullSpeedUU = 1800.f;

	/** Close enough = done. Above the capsule radius so arrival cannot oscillate. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grapple")
	float GrappleArrivalRadiusUU = 120.f;

	/** The pull's hard ceiling — a snagged pull ends instead of dragging forever. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grapple")
	float GrappleMaxPullSeconds = 1.5f;

	uint8 bWantsToGrapple : 1;

private:
	void ClearGrapplePullState();

	uint16 ActiveGrapplePullSourceID = 0;
	FVector GrapplePullTarget = FVector::ZeroVector;

	/** Set when a grapple intent bit has been SEEN this pull — the guard that lets a
	 *  replay distinguish "flag not yet arrived" from "flag deliberately dropped". */
	uint8 bGrappleIntentObserved : 1;

	friend struct FSavedMove_BN;
};
