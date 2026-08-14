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
 * Which component of a BONE-SPACE rotator carries an angle.
 *
 * The aim and lean chains are Transform(Modify)Bone nodes running in bone space on spine_01..05
 * plus the neck and head. A spine bone's local axes are NOT the world's, so "pitch the torso up"
 * is not FRotator::Pitch — on this skeleton it is Roll, which is why the asset's own graph builds
 * the aim rotator as MakeRotator(Pitch) into the FIRST pin. This is a measured property of the
 * Manny rig, not a derivable one, so it is data: BNAimAxis / BNLeanAxis flip it live in PIE
 * rather than costing a rebuild per guess.
 */
UENUM()
enum class EBNSpineAxis : uint8
{
	Roll  UMETA(DisplayName = "Roll (X)"),
	Pitch UMETA(DisplayName = "Pitch (Y)"),
	Yaw   UMETA(DisplayName = "Yaw (Z)")
};

/** One angle on one axis; every other component stays zero. */
FORCEINLINE FRotator BNMakeAxisRotator(EBNSpineAxis Axis, double Degrees)
{
	switch (Axis)
	{
	case EBNSpineAxis::Pitch: return FRotator(Degrees, 0.0, 0.0);
	case EBNSpineAxis::Yaw:   return FRotator(0.0, Degrees, 0.0);
	default:                  return FRotator(0.0, 0.0, Degrees);
	}
}

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

	/** The PIE probe R3 has been owed since the aim chain went dark: one line naming every value
	 *  in the chain, so "the pose does not move" stops being a guess about which link is zero. */
	FString DescribeAimState() const;

	/** Live axis flips for the founder's playtest — the bone-space answer is measured, not
	 *  derived, and a rebuild per guess is the wrong price. */
	void SetAimPitchAxis(EBNSpineAxis Axis) { AimPitchAxis = Axis; }
	void SetLeanAxis(EBNSpineAxis Axis) { LeanAxis = Axis; }

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

	/** The asset's OWN name for its ADS flag — the pack modelled ADS as a gameplay-tag mirror
	 *  before BN existed (old module port, ABPMannequinBase.h:351), and State.Weapon.ADS is that
	 *  tag. Ungated: the event graph's only writer is the never-fired SetADS interface event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool GameplayTag_IsADS = false;

	/** The asset's ADS edge — CrouchStateChange's class exactly, so it publishes only under
	 *  bNativeOwnsTurnState: the live event graph still runs the same edge math, and two writers
	 *  of an edge double-fire it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	bool ADSStateChanged = false;

	/** The reference's measured AimFOV (MyCharacter.h:344) and AimPoseChangeSpeed (:353). The blend
	 *  runs in NativeUpdateAnimation, owner-only — the per-frame presentation brain, not a new
	 *  tick. The base FOV is captured from the camera on first sight, never hardcoded. */
	UPROPERTY(EditAnywhere, Category = "BN")
	float ADSFOV = 80.f;

	UPROPERTY(EditAnywhere, Category = "BN")
	float ADSFOVInterpSpeed = 18.f;

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

	/** THE aim property. The linked aim layers (FSP_FullBody_Aiming_Pitch_FPS_Upper/_Neck) do not
	 *  read `Pitch` — their ModifyBone rotation pins bind to `GetMainAnimBPThreadSafe.PitchRotator`,
	 *  and until now nothing in C++ declared it. The asset's UpdateRotationData built it from
	 *  `Pitch`, but that is a Blueprint hop BN cannot verify from source and evidently is not
	 *  landing; writing the rotator directly removes the hop. `Pitch` stays published because the
	 *  asset's own graph still reads it, so both halves agree either way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN")
	FRotator PitchRotator = FRotator::ZeroRotator;

	/** Bone-space axis that bends the spine up/down. Roll is the asset's own answer (its
	 *  MakeRotator(Pitch) fills pin 1); flip live with `BNAimAxis 0|1|2` if the torso bends
	 *  the wrong way. */
	UPROPERTY(EditAnywhere, Category = "BN")
	EBNSpineAxis AimPitchAxis = EBNSpineAxis::Roll;

	UPROPERTY(EditAnywhere, Category = "BN")
	FVector2D AimPitchClamp = FVector2D(-90.0, 90.0);

	/** The yield switch, founder's ruling 14 Aug: with the template's procedural components on
	 *  the character (BPC_FPSComp sending SetControllerPitch/SetFPSMode, the Manager sending
	 *  SetAimAndLeanInfo), the ASSET's interface events own Pitch/PitchRotator/bFPSMode/lean —
	 *  and this native publisher, which runs after them, must stand down or it stomps every
	 *  message. True = the R3-G1 native path (no components needed); false = the template's
	 *  own communications drive the pose. Set on the ABP's defaults, not per instance. */
	UPROPERTY(EditAnywhere, Category = "BN")
	bool bNativeOwnsAimSurface = true;

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

	/** Bone-space axis that tips the torso sideways — a DIFFERENT axis from the aim bend, since
	 *  the two motions are perpendicular on the same bones. Roll was the first guess and the
	 *  founder's playtest reported it as "a little up and down", i.e. it produced the aim bend
	 *  instead of a lean; Yaw is the remaining cross-axis. `BNLeanAxis 0|1|2` settles it live. */
	UPROPERTY(EditAnywhere, Category = "BN")
	EBNSpineAxis LeanAxis = EBNSpineAxis::Yaw;

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
	FDelegateHandle ADSTagHandle;

	bool bTagInAir = false;
	bool bTagCrouching = false;
	bool bTagJumping = false;
	bool bTagSprinting = false;
	bool bTagLeanLeft = false;
	bool bTagLeanRight = false;
	bool bTagADS = false;

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
	bool bSnapADS = false;

	// Worker-thread history.
	FFloatSpringState RootYawOffsetSpringState;
	double PreviousTurnYawCurveValue = 0.0;
	double PreviousYaw = 0.0;
	FVector PreviousWorldLocation = FVector::ZeroVector;
	bool bWasCrouchingLastUpdate = false;
	bool bWasADSLastUpdate = false;
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
	bool bNativeADSStateChange = false;
	bool bNativeLinkedLayerChanged = false;
	FVector NativePivotDirection2D = FVector::ZeroVector;
	uint8 NativeCardinalFromAcceleration = 0;
	double NativeUpperbodyAdditiveWeight = 0.0;
	double NativeCurrLeaning = 0.0;
	uint8 NativeLocalVelocityDirection = 0;
	uint8 NativeLocalVelocityDirectionNoOffset = 0;

	/** The camera's own FOV, captured on first sight; negative = not yet captured. */
	float DefaultFOV = -1.f;
};
