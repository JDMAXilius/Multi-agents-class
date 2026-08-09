#pragma once
// EOSCombatState (Idle, Attacking, Blocking, etc.). Used by anim instance and UI to read current combat state.

#include "CoreMinimal.h"
#include "GameplayTags.h"
#include "OSCombatTypes.generated.h"

UENUM(BlueprintType)
enum class EOSCombatState : uint8
{
	Idle,
	Attacking,
	Blocking,
	Dodging,
	Stunned,
	Dead,
	HitReacting,
	KnockedDown,
	Grabbed,
	Grabbing,
	Charging,
	Mantling,
	Casting,
	Recoiling
};

/**
 * Attack Phase — which temporal phase of the attack is currently active.
 * Driven by GA_OSAttack::SetPhase() in response to ANS_OSAttackPhase notify signals.
 * External systems query via gameplay tags (Gameplay.State.Attack.Windup/Active/Recovery).
 */
UENUM(BlueprintType)
enum class EOSAttackPhase : uint8
{
	None,       // Ability not active or between attacks
	Windup,     // Section start → NotifyBegin (telegraph, MW lunge)
	Active,     // NotifyBegin → NotifyEnd (hitbox live, traces active)
	Recovery    // NotifyEnd → section end / blend-out (punishable)
};

/** Cardinal locomotion direction — which quadrant the character is moving relative to facing. */
UENUM(BlueprintType)
enum class EOSLocomotionDirection : uint8
{
	Forward,
	Backward,
	Left,
	Right
};

/** Hip orientation relative to movement — which way the upper body faces during strafing. */
UENUM(BlueprintType)
enum class EOSHipFacing : uint8
{
	Forward,
	Backward
};

/** Locomotion gait — determines animation set and movement feel.
  * Derived from tags + speed: IsSprinting → Sprint, IsWalking → Walk, else speed threshold. */
UENUM(BlueprintType)
enum class EOSGait : uint8
{
	Walk,
	Jog,
	Sprint
};

/** Root yaw offset mode — controls how the mesh-to-actor rotation gap is managed.
  * Accumulate: idle, counter-rotate mesh to hold facing. Hold: stopping, freeze offset.
  * BlendOut: moving, spring-interp offset back to zero. */
UENUM(BlueprintType)
enum class EOSRootYawOffsetMode : uint8
{
	BlendOut,   // Default: smoothly return offset to zero (moving states)
	Accumulate, // Counter-rotate mesh as actor yaw changes (idle)
	Hold        // Freeze offset at current value (stop state)
};

/** Angle thresholds for locomotion direction with hysteresis deadzone. */
USTRUCT(BlueprintType)
struct ONSIGHT_API FOSLocomotionDirectionSettings
{
	GENERATED_BODY()

	/** Angles within [-ForwardMax, ForwardMax] count as Forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (ClampMin = "0", ClampMax = "90"))
	float ForwardMax = 50.f;

	/** Angles beyond +/-BackwardMin count as Backward (handles -180/+180 wraparound). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (ClampMin = "90", ClampMax = "180"))
	float BackwardMin = 130.f;

	/** Widens current direction's threshold to prevent flickering at boundaries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (ClampMin = "0", ClampMax = "30"))
	float DeadzoneDegrees = 10.f;
};

/** Which foot currently bears weight. */
UENUM(BlueprintType)
enum class EOSStanceFoot : uint8
{
	Left,
	Right,
	Both,     // Double-support phase or stationary
	Neither   // Airborne
};

/** Per-foot raw data computed each frame. Two-tier sourcing: anim curves preferred, bone position fallback. */
USTRUCT(BlueprintType)
struct ONSIGHT_API FOSFootState
{
	GENERATED_BODY()

	/** 0.0 = airborne/swing, 1.0 = fully planted.
	  * Tier 1: From Foot_L/Foot_R curves. Tier 2: From bone height delta. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	float PlantWeight = 0.f;

	/** Foot bone height relative to root in component space (cm). */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	float Height = 0.f;

	/** Forward offset along character facing (cm). Positive = ahead of root. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	float ForwardOffset = 0.f;

	/** Foot bone speed (cm/s). Matches what FootPlacement reads from SpeedCurveName. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	float Speed = 0.f;

	/** Foot bone rotation in component space. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	FRotator BoneRotationCS = FRotator::ZeroRotator;

	// --- Per-Foot Ground Data (Tier 1: curves, Tier 2: per-foot line trace) ---

	/** Ground normal under this foot in world space. For ankle alignment and slope-adaptive knee direction. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	FVector GroundNormal = FVector::UpVector;

	/** Distance from foot bone to ground (cm). 0 = on ground. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	float GroundDistance = 0.f;

	/** Rotation delta to align foot flat onto the ground surface. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	FRotator GroundAlignmentDelta = FRotator::ZeroRotator;

	/** IK target location in component space — the ground plant point for this foot. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	FVector IKTargetCS = FVector::ZeroVector;

	// --- Pole / Knee Direction ---

	/** Knee/pole direction hint in component space. FK knee forward, adjusted by ground slope. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	FVector PoleDirectionCS = FVector::ForwardVector;

	/** Pole target in component space (knee bone + PoleOffset along PoleDirectionCS). */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	FVector PoleTargetCS = FVector::ZeroVector;

	/** Pole target offset distance (cm) from knee bone along PoleDirectionCS. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	float PoleOffset = 50.f;

	// --- IK Policy ---

	/** IK enable alpha (0 = off, 1 = on). Starts at 0, blends in on spawn. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	float IKAlpha = 0.f;

	/** Gameplay lock hint. 0 = FootPlacement decides. 1 = force locked. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot")
	float LockHint = 0.f;
};

/** Complete stance state: per-foot data + derived stride state + IK policy. */
USTRUCT(BlueprintType)
struct ONSIGHT_API FOSStanceState
{
	GENERATED_BODY()

	// --- Per-Foot Raw Data ---

	UPROPERTY(BlueprintReadOnly, Category = "Stance")
	FOSFootState LeftFoot;

	UPROPERTY(BlueprintReadOnly, Category = "Stance")
	FOSFootState RightFoot;

	// --- Derived Stride State ---

	/** Which foot currently bears weight. */
	UPROPERTY(BlueprintReadOnly, Category = "Stance")
	EOSStanceFoot StanceFoot = EOSStanceFoot::Both;

	/** Current stride state: true = left foot ahead. Hysteresis-protected, freezes when stopped. */
	UPROPERTY(BlueprintReadOnly, Category = "Stance")
	bool bLeftFootForward = true;

	/** Snapshot at the moment movement stopped. For distance-matched start animation selection. */
	UPROPERTY(BlueprintReadOnly, Category = "Stance")
	bool bLeftFootForwardAtStop = true;

	/** True during the passing phase of the gait cycle (feet crossing). */
	UPROPERTY(BlueprintReadOnly, Category = "Stance")
	bool bFeetCrossing = false;

	/** Normalized gait cycle position (0-1). 0/1 = left contact, 0.5 = right contact. */
	UPROPERTY(BlueprintReadOnly, Category = "Stance")
	float StridePhase = 0.f;

	// --- Convenience Booleans (derived from PlantWeight > threshold) ---

	UPROPERTY(BlueprintReadOnly, Category = "Stance")
	bool bLeftFootDown = true;

	UPROPERTY(BlueprintReadOnly, Category = "Stance")
	bool bRightFootDown = false;

	// --- Foot IK Policy ---

	/** Master foot IK enable alpha. Starts at 0, blends in on spawn. */
	UPROPERTY(BlueprintReadOnly, Category = "Stance")
	float FootIKAlpha = 0.f;
};

/** Pre-computed lean angles for additive lean blendspace layers. */
USTRUCT(BlueprintType)
struct ONSIGHT_API FOSBodyLeanState
{
	GENERATED_BODY()

	/** Forward/backward lean from acceleration. -1 to 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Lean")
	float ForwardLean = 0.f;

	/** Left/right lean from lateral acceleration. -1 to 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Lean")
	float SideLean = 0.f;

	/** Turn lean from yaw rotation rate. -1 to 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Lean")
	float TurnLean = 0.f;
};

/**
 * Attack Type - Classification for damage calculation
 * Used for: Damage scaling, animation selection, property hints
 * NOT used for: State (use tags), Direction (use tags)
 */
UENUM(BlueprintType)
enum class EOSAttackType : uint8
{
	None            UMETA(DisplayName = "None"),
	Light           UMETA(DisplayName = "Light Attack"),
	Heavy           UMETA(DisplayName = "Heavy Attack"),
	Special         UMETA(DisplayName = "Special Attack"),  // Grabs, launchers, aura
	Fatal           UMETA(DisplayName = "Fatal/Finisher")
};

/**
 * Target Lock Mode - For target lock component only
 * Used for: Lock behavior in TargetLockComponent
 * NOT used for: State (use tags instead)
 */
UENUM(BlueprintType)
enum class EOSTargetLockMode : uint8
{
	None            UMETA(DisplayName = "No Lock"),
	Soft            UMETA(DisplayName = "Soft Lock"),     // Targeting assist
	Hard            UMETA(DisplayName = "Hard Lock")      // Camera locked
};

/** Blending algorithm when combining target direction with input direction (attack soft lock). */
UENUM(BlueprintType)
enum class EOSBlendAlgorithm : uint8
{
	/** Linear Lerp: simple interpolation. Input freely pulls away from target. */
	LinearLerp,
	/** Magnetic: target pulls with spring-like resistance. Breakaway threshold before input overrides. */
	Magnetic,
	/** Weighted Angle: dead zone near target, power curve ramp for input influence. */
	WeightedAngle,
	/** Cone Clamp: input moves freely within cone, hard clamp at edge. */
	ConeClamp
};

/** Parameters for ResolveAttackDirectionInCone (cone search + direction blend). */
USTRUCT(BlueprintType)
struct ONSIGHT_API FOSAttackDirectionParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "10", ClampMax = "180"))
	float ConeAngleDegrees = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "50"))
	float MaxRange = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FGameplayTagContainer ExcludeTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	EOSBlendAlgorithm BlendAlgorithm = EOSBlendAlgorithm::Magnetic;

	// --- LinearLerp ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0", ClampMax = "1"))
	float InputInfluence = 0.15f;

	// --- Magnetic ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0", ClampMax = "1"))
	float MagneticStickiness = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "5", ClampMax = "180"))
	float MagneticBreakawayAngle = 60.f;

	// --- WeightedAngle ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0", ClampMax = "90"))
	float DeadZoneAngle = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "10", ClampMax = "180"))
	float FullControlAngle = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.1", ClampMax = "5"))
	float BlendCurveExponent = 2.f;

	// --- ConeClamp ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "5", ClampMax = "180"))
	float MaxDeviationAngle = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0", ClampMax = "1"))
	float ConeInputWeight = 1.f;

};

