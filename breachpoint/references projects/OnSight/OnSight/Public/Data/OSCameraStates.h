// Camera state params (Chooser input) and camera config data asset (Chooser output).
// FOSCameraStateParams: evaluated by the Chooser Table each tick.
// UOSCameraConfigDataAsset: returned by the Chooser Table; used by OSCameraComponent (blend presets) and OSCameraAutoFollowComponent (framing + sweep/autofollow).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Curves/CurveFloat.h"
#include "Data/OSPrimaryDataAssets.h"
#include "OSCameraStates.generated.h"

// =============================================================================
// Chooser Input: current character state tags, fed to the Chooser Table columns
// =============================================================================

USTRUCT(BlueprintType)
struct FOSCameraStateParams
{
	GENERATED_BODY()

	/** Current gameplay tags on the character's ASC. The Chooser Table columns
	 *  evaluate against this container to pick the correct camera config. */
	UPROPERTY(BlueprintReadOnly)
	FGameplayTagContainer ActiveTags;
};

// =============================================================================
// Chooser Output: camera configuration data asset
// =============================================================================

UCLASS(BlueprintType)
class ONSIGHT_API UOSCameraConfigDataAsset : public UOSDataAsset
{
	GENERATED_BODY()

public:
	// ========================================
	// FRAMING
	// ========================================

	/** Spring arm length (boom distance from character). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing")
	float BoomLength = 350.0f;

	/** Camera field of view (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing")
	float CameraFOV = 90.0f;

	/** Vertical offset from the spring arm socket (SocketOffset.Z). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing")
	float CameraHeight = 60.0f;

	/** Horizontal offset from the spring arm socket (SocketOffset.Y). Positive = right shoulder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing")
	float CameraSideOffset = 0.0f;

	/** Pitch offset added to the spring arm rotation. Negative = look down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing")
	float CameraPitchOffset = 0.0f;

	// ========================================
	// TRANSITION / BLEND (OSCameraComponent + AutoFollow)
	// ========================================

	/** How fast the camera interpolates toward these values when this config becomes active.
	 *  Higher = snappier. Typical range: 2.0 (slow, cinematic) to 12.0 (fast, responsive). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0.1", ClampMax = "30.0"))
	float TransitionSpeed = 6.0f;

	/** Spring arm positional lag speed. Higher = snappier. Used by OSCameraComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0.1"))
	float CameraLagSpeed = 10.0f;

	/** Spring arm rotational lag speed. Higher = snappier. Used by OSCameraComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0.1"))
	float CameraRotationLagSpeed = 10.0f;

	/** Blend duration (seconds) when switching to this preset. Used by OSCameraComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0.01"))
	float BlendTime = 0.3f;

	/** Optional curve for blend alpha over BlendTime. X: normalized time [0..1], Y: alpha [0..1]. Used by OSCameraComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	TObjectPtr<UCurveFloat> BlendCurve;

	// ========================================
	// LOCOMOTION SWEEP
	// ========================================

	/** Whether locomotion-triggered camera sweep is enabled in this state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion Sweep")
	bool bEnableLocomotionSweep = true;

	/** Camera yaw sweep speed (degrees/sec) during locomotion in this state.
	 *  For sprint states, set this higher than jog states. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion Sweep", meta = (EditCondition = "bEnableLocomotionSweep"))
	float LocomotionSweepSpeed = 30.0f;

	/** Minimum lateral movement angle (degrees from camera forward) to trigger sweep. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion Sweep", meta = (EditCondition = "bEnableLocomotionSweep"))
	float LocomotionSweepMinAngle = 15.0f;

	/** Interpolation speed for sweep ramp-up/down (higher = snappier start). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion Sweep", meta = (EditCondition = "bEnableLocomotionSweep"))
	float LocomotionSweepAcceleration = 4.0f;

	// ========================================
	// COMBAT SWEEP
	// ========================================

	/** Whether combat soft-lock camera sweep is enabled in this state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sweep")
	bool bEnableCombatSweep = false;

	/** Degrees the camera sweeps toward the soft-lock target per attack event. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sweep", meta = (EditCondition = "bEnableCombatSweep"))
	float CombatSweepDegreesPerAttack = 15.0f;

	/** Max sweep speed (degrees/sec) during combat sweep interpolation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sweep", meta = (EditCondition = "bEnableCombatSweep"))
	float CombatSweepSpeed = 60.0f;

	// ========================================
	// AUTO-FOLLOW
	// ========================================

	/** Whether auto-follow-behind is enabled in this state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto-Follow")
	bool bEnableAutoFollow = true;

	/** Speed (degrees/sec) the camera rotates to settle behind the character when idle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto-Follow", meta = (EditCondition = "bEnableAutoFollow"))
	float AutoFollowSpeed = 45.0f;

	/** Minimum angle offset before auto-follow activates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto-Follow", meta = (EditCondition = "bEnableAutoFollow"))
	float AutoFollowDeadzone = 5.0f;

	/** How long (seconds) the character must be idle before auto-follow begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Auto-Follow", meta = (EditCondition = "bEnableAutoFollow"))
	float AutoFollowIdleDelay = 1.5f;
};
