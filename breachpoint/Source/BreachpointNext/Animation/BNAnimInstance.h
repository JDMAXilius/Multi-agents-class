#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetMathLibrary.h"
#include "BNAnimInstance.generated.h"

class ABNCharacter;
class UCharacterMovementComponent;
class UAbilitySystemComponent;

/**
 * The mannequin ABP's per-frame brain, ported from the ABP_Mannequin_Base record so the
 * duplicate's event graph can be cleared. Two passes, never mixed: NativeUpdateAnimation
 * (game thread) snapshots UObject state; NativeThreadSafeUpdateAnimation (worker) is the
 * C++ writer of every graph-facing property — RMW/edge fields publish only under
 * bNativeOwnsTurnState, because the live event graph still runs the same accumulators.
 *
 * Properties are BlueprintReadWrite for the migration window only — the un-cleared event
 * graph must still be able to set them. Once the graph is cleared, C++ is the sole writer.
 * Names are the asset's (including `isCrouching`): a name is how a property survives the
 * reparent, so they are not tidied.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;

protected:
	// false = the live event graph owns the RMW/edge accumulator outputs (today); true = C++
	// owns them — set when the graph is cleared. Native always computes them against private
	// history either way; this only gates the publish.
	UPROPERTY(EditAnywhere, Category = "BN")
	bool bNativeOwnsTurnState = false;

	// ---------------------------------------------------------------- locomotion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	FVector LocalVelocity2D = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	FVector LocalAcceleration2D = FVector::ZeroVector;

	/** Distance covered per second — the source's distance-matching input, NOT |velocity|:
	 *  a slide to a stop keeps velocity long after it stops covering ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double DisplacementSpeed = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double DisplacementSinceLastUpdate = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double LocalVelocityDirectionAngle = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double LocalVelocityDirectionAngleWithOffset = 0.0;

	// The enum-typed outputs (LocalVelocityDirection/NoOffset, CardinalDirectionFromAcceleration,
	// RootYawOffsetMode) have NO published UPROPERTY: the graph's asset-enum pins cannot bind a
	// byte property. They stay BP-owned until graph-clear day, when they land as UENUMs with the
	// graph retyped in the same change. Native computes them fully against private members below.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	FVector PivotDirection2D = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double CardinalDirectionDeadZone = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool HasVelocity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool HasAcceleration = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool IsRunningIntoWall = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool IsFirstUpdate = true;

	// ---------------------------------------------------------------- air
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool IsFalling = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool IsOnGround = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool IsJumping = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double TimeToJumpApex = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double GroundDistance = 0.0;

	// ---------------------------------------------------------------- crouch
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool isCrouching = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool CrouchStateChange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double ApplyCrouchAlpha = 0.0;

	// ---------------------------------------------------------------- weapon / pose selection
	/** The upper-body layer's pose selector. Name is the asset's — ABP_Mannequin_Base's
	 *  `bUnarmed`, which its BPI_FPST_AnimInterface `SetUnarmed` event used to write. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool bUnarmed = false;

	/** The upper body's sprint pose selector (the `fPS_Sprint` slot). Name is the asset's —
	 *  ABP_Mannequin_Base's `bSprinting`, which BPI_FPST_AnimInterface's `SetSprinting` wrote.
	 *  Driven here by State.Movement.Sprinting; that tag is what replaces the message. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool bSprinting = false;

	// ---------------------------------------------------------------- aim, lean
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double AimPitch = 0.0;

	/** The asset's `Pitch` — what BPI_FPST_AnimInterface's SetControllerPitch(InPitch) used to
	 *  write, and what nothing has written since. The aim-offset side of the pose reads it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double Pitch = 0.0;

	/** The asset's `bFPSMode` — the BlendListByBool gate in front of EVERY aim-pitch layer.
	 *  The template's CAMERA component fired SetFPSMode on view change; BN has no camera
	 *  component and is first-person only, so the rule collapses to the same per-instance
	 *  answer the template produced: my own pawn poses FPS, every other machine's view of me
	 *  poses third person. BlueprintReadWrite so the asset's now-dead SetFPSMode event setter
	 *  still compiles once its BP-local duplicate variable is removed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool bFPSMode = false;

	UPROPERTY(EditAnywhere, Category = "BN")
	FVector2D AimPitchClamp = FVector2D(-90.0, 90.0);

	/** Written only through SetRootYawOffset (= -RootYawOffset), as the source does. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double AimYaw = 0.0;

	/** The spine's lean, and the head's counter-lean — the two rotations SetAimAndLeanInfo
	 *  carried as InLeanRotation / InOppLeanRotation. The asset's LeanSpineWeights_UE4/UE5 share
	 *  them out per bone; the head entry there is labelled "Opposite Angle Weight", which is what
	 *  the second rotation is for: the body tips, the gaze stays put. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	FRotator LeanRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	FRotator LeanOppRotation = FRotator::ZeroRotator;

	/** Source measurements: max lean 12 degrees, chased by an interp at 8 (not a spring — lean
	 *  settles without overshoot). */
	UPROPERTY(EditAnywhere, Category = "BN")
	double LeanAngle = 12.0;

	UPROPERTY(EditAnywhere, Category = "BN")
	double LeanInterpSpeed = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double YawDeltaSinceLastUpdate = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double YawDeltaSpeed = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double AdditiveLeanAngle = 0.0;

	// ---------------------------------------------------------------- root yaw offset / turn in place
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double RootYawOffset = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	FVector2D RootYawOffsetAngleClamp = FVector2D(-120.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	FVector2D RootYawOffsetAngleClampCrouched = FVector2D(-90.0, 80.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool bEnableRootYawOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double TurnYawCurveValue = 0.0;

	// ---------------------------------------------------------------- additives / layers
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	double UpperbodyDynamicAdditiveWeight = 0.0;

	/** One-frame edge: the mesh's linked layer class changed since last update. The CHARACTER
	 *  owns the linking; this instance only observes the swap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool LinkedLayerChanged = false;

private:
	void ResolveAbilitySystem();
	void OnTagChanged(const FGameplayTag Tag, int32 NewCount);

	// Worker-thread helpers, ported 1:1 from the source's function graphs. Both operate on
	// the PRIVATE accumulators; shared properties are only touched in the gated publish.
	void SetRootYawOffset(double InRootYawOffset);
	void ProcessTurnYawCurve();

	// Game thread. Walking answers 0 without a trace; airborne traces down.
	double ComputeGroundDistance() const;

	UPROPERTY(Transient)
	TObjectPtr<ABNCharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	/** Last linked-layer class seen on the mesh — compare-only, for the LinkedLayerChanged edge. */
	UPROPERTY(Transient)
	TObjectPtr<UClass> ObservedLayerClass;

	FDelegateHandle CrouchTagHandle;
	FDelegateHandle JumpTagHandle;
	FDelegateHandle InAirTagHandle;
	FDelegateHandle SprintTagHandle;
	FDelegateHandle LeanLeftTagHandle;
	FDelegateHandle LeanRightTagHandle;

	bool bTagInAir = false;
	bool bTagCrouching = false;
	bool bTagJumping = false;
	bool bTagSprinting = false;
	bool bTagLeanLeft = false;
	bool bTagLeanRight = false;

	// Game-thread snapshot, worker-thread input. The only channel between the two passes.
	FVector SnapWorldVelocity = FVector::ZeroVector;
	FVector SnapWorldAcceleration = FVector::ZeroVector;
	FVector SnapWorldLocation = FVector::ZeroVector;
	FRotator SnapRotation = FRotator::ZeroRotator;
	double SnapGravityZ = -980.0;
	double SnapBaseAimPitch = 0.0;
	double SnapGroundDistance = 0.0;
	bool bSnapAnyMontagePlaying = false;
	bool bSnapLayerChanged = false;
	bool bSnapFalling = false;
	bool bSnapInAirTag = false;
	bool bSnapCrouching = false;
	bool bSnapJumping = false;
	bool bSnapUnarmed = false;
	bool bSnapSprinting = false;
	bool bSnapFPSMode = false;
	bool bSnapLeanLeft = false;
	bool bSnapLeanRight = false;

	// Worker-thread history.
	FFloatSpringState RootYawOffsetSpringState;
	double PreviousTurnYawCurveValue = 0.0;
	double PreviousYaw = 0.0;
	FVector PreviousWorldLocation = FVector::ZeroVector;
	bool bWasCrouchingLastUpdate = false;
	bool bNativeWasMoving = false;

	// Private accumulators/edges — native math NEVER reads a shared property back; while the
	// event graph is live it repeats these RMWs on the shared fields, so two live writers on
	// one accumulator would double-count. Published only under bNativeOwnsTurnState.
	bool bNativeFirstUpdate = true;
	double NativeRootYawOffset = 0.0;
	uint8 NativeRootYawOffsetMode = 0;
	double NativeTurnYawCurveValue = 0.0;
	double NativeYawDeltaSinceLastUpdate = 0.0;
	double NativeYawDeltaSpeed = 0.0;
	bool bNativeCrouchStateChange = false;
	bool bNativeLinkedLayerChanged = false;
	FVector NativePivotDirection2D = FVector::ZeroVector;
	uint8 NativeCardinalFromAcceleration = 0;
	double NativeUpperbodyAdditiveWeight = 0.0;
	double NativeCurrLeaning = 0.0;
	uint8 NativeLocalVelocityDirection = 0;
	uint8 NativeLocalVelocityDirectionNoOffset = 0;
};
