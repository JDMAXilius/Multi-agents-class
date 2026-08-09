// GA_OSmantleability.h
// Gameplay Ability for mantling onto surfaces in OnSight
// Integrates with Motion Warping for smooth traversal animations
//
// Design: All detection thresholds are DERIVED from the character's capsule geometry.
// No manual tuning of heights, offsets, or distances -- the system adapts to any capsule size.
// Only genuine gameplay/design choices remain as UPROPERTYs.
//
// Detection: Overlap sphere finds all nearby geometry, then per-component targeted
// traces build candidate mantle targets. Candidates are scored and the best is chosen.
//
// MW Robustness: Character is pre-positioned at the wall face before MW starts,
// minimizing warp deltas. Maximum delta clamping rejects unreachable targets.
//
// Three mantle types:
//   Vault -- lateral over thin obstacle, land on far side (never stand on top)
//   Climb -- from below/in front, land on top of obstacle (grounded only)
//   Catch -- airborne, grab ledge, pull up onto top

#pragma once

#include "CoreMinimal.h"
#include "Data/OSMantleTypes.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GA_OSmantleability.generated.h"

class UMotionWarpingComponent;
class UAnimMontage;
struct FOSMantleDetectionConfig;

// Data types (EOSMantleType, FOSCharacterGeo, FOSObstacleGeo, FOSMantleTraceResult,
// FOSMantleDebugData) are defined in Data/OSMantleTypes.h.

/**
 * Gameplay Ability that handles mantling/climbing up onto ledges.
 * Uses Motion Warping for smooth animations.
 *
 * Detection uses an overlap sphere to find ALL nearby geometry, then evaluates
 * each surface as a mantle candidate. Candidates are scored by distance, angle,
 * warp delta, and height. The best candidate is chosen.
 *
 * Three mantle types with 5 montage slots:
 *   Vault:      VaultMontage (go over, land on far side)
 *   Climb Low:  ClimbLowMontage (knee-to-waist height, land on top, grounded)
 *   Climb High: ClimbHighMontage (waist+ height, land on top, grounded)
 *   Catch Low:  CatchLowMontage (airborne, feet < waist below ledge)
 *   Catch High: CatchHighMontage (airborne, feet >= waist below ledge)
 *
 * MW Robustness: Character is pre-positioned at the wall face before MW runs,
 * so warp deltas are small and predictable. MaxEdgeWarpDelta rejects unreachable targets.
 */
UCLASS()
class ONSIGHT_API UGA_OSmantleability : public UOSGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_OSmantleability();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // Defensive cleanup path for when the ability spec is removed without EndAbility firing first.
    // Mirrors EndAbility's character-dependent cleanup but unconditionally resets flags and
    // clears warp targets, so the ASC can reactivate cleanly after a spec-level removal.
    virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;

	// Used by other abilities to cancel mantle without snapping back to pre-mantle transform.
	// (Example: dodge cancels mantle for responsiveness; death/grab/stun still roll back.)
	void SetSkipRollbackOnCancel(bool bSkip) { bSkipRollbackOnCancel = bSkip; }

#if !UE_BUILD_SHIPPING
    /** Run a full mantle trace and cache debug data. Called by OSHUD every frame when MantleViz is active. */
    FOSMantleDebugData DebugRunMantleTrace() const;
#endif

protected:
    // --- Animation (5 montage slots) ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation")
    UAnimMontage* VaultMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation")
    UAnimMontage* ClimbLowMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation")
    UAnimMontage* ClimbHighMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation")
    UAnimMontage* CatchLowMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation")
    UAnimMontage* CatchHighMontage;

    // --- Motion Warping ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Motion Warping")
    FName EdgeWarpTargetName = "MantleEdge";

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Motion Warping")
    FName LandingWarpTargetName = "MantleTarget";

    // Maximum distance MW can warp the character to reach the edge target (after pre-positioning).
    // Mantles exceeding this are rejected. 0 = no limit.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Motion Warping", meta=(ClampMin="0.0"))
    float MaxEdgeWarpDelta = 300.f;

    // Maximum distance MW can warp from edge to landing target. 0 = no limit.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Motion Warping", meta=(ClampMin="0.0"))
    float MaxLandingWarpDelta = 100.f;

    // --- Detection ---

    // Maximum height the player can mantle (gameplay cap, not geometry-derived)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection")
    float MaxMantleHeight = 250.0f;

    // Overlap sphere radius for detecting nearby geometry. 0 = auto (4x CapsuleRadius).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection", meta=(ClampMin="0.0"))
    float DetectionSphereRadius = 0.f;

    // Maximum angle (degrees) from character forward to consider a wall candidate.
    // 180 = full sphere (detect behind), 90 = forward hemisphere, 45 = tight cone.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection", meta=(ClampMin="30.0", ClampMax="180.0"))
    float MaxDetectionAngle = 120.f;

    // Number of vertical slices for wall profiling in EvaluateWallHit.
    // More slices = more accurate wall top and inset detection, but more line traces per candidate.
    // 6 slices from feet to MaxMantleHeight gives ~42cm spacing — catches most features.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection", meta=(ClampMin="4", ClampMax="10"))
    int32 ProfileSliceCount = 6;

    // Distance variance threshold (cm) for inset detection in the vertical profile.
    // If the max distance difference between profile slices exceeds this, the obstacle
    // has an inset/protruding base. Affects which normal is used for approach direction.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection", meta=(ClampMin="1.0", ClampMax="50.0"))
    float InsetDetectionThreshold = 10.f;

    // Maximum number of wall candidates to fully evaluate per trace. Limits cost in dense scenes.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection", meta=(ClampMin="1", ClampMax="10"))
    int32 MaxCandidates = 5;

    // Sphere sweep radius for per-candidate wall trace. Larger = catches beveled edges. 0 = line trace.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection")
    float SweepTraceRadius = 5.f;

    // Trace channel for mantle detection
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection")
    TEnumAsByte<ECollisionChannel> MantleTraceChannel = ECC_Visibility;

    // Maximum surface angle (degrees from up) considered mantleable. 0 = use CMC's WalkableFloorAngle.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection", meta=(ClampMin="0.0", ClampMax="60.0"))
    float MaxSurfaceAngle = 0.f;

    // Overhead clearance margin (cm) above the capsule top at the landing position.
    // The capsule clearance check already verifies the character's body fits.
    // This additional margin catches geometry immediately above the head (low ceilings,
    // overhangs) that the capsule sweep might miss at the exact check position.
    // Too large = rejects mantles under normal roofs. Too small = clips through low ceilings.
    // Default 30cm: enough for arms-overhead climbing animation headroom.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection", meta=(ClampMin="0.0", ClampMax="200.0"))
    float OverheadClearanceMargin = 30.f;

    // If true, CLIMBS require airborne (jumping/falling). Catches are always airborne. Vaults always work.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection")
    bool bRequireFalling = false;

    // Minimum vertical velocity for climb when bRequireFalling is true.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection", meta=(EditCondition="bRequireFalling"))
    float MinVerticalVelocity = -100.0f;

    // --- Jump vs Mantle Arbitration ---

    // Fraction of max jump height below which CLIMB candidates defer to jump.
    // Computed threshold = MaxJumpHeight * JumpClearanceFactor.
    // Obstacles at or below this height are assumed clearable by jumping.
    // Vaults and Catches are unaffected. 0 = all climbs mantle (legacy behavior).
    // Default 0.4 = 100cm (waist height) — conservative to avoid dead zones
    // where jump is chosen but can't practically clear the obstacle.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Arbitration", meta=(ClampMin="0.0", ClampMax="1.0"))
    float JumpClearanceFactor = 0.4f;

    // Minimum character velocity (cm/s) toward the obstacle for VAULT activation.
    // Uses InputDirection (pure player input) for direction check, combined with
    // physics velocity for speed check. Prevents knockback-triggered vaults.
    // Catches (airborne) and tall Climbs are unaffected. 0 = no speed requirement.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Arbitration", meta=(ClampMin="0.0"))
    float MinApproachSpeed = 150.f;

    // Minimum dot product between player InputDirection and wall-toward direction
    // for VAULT activation. Controls how precisely the player must aim at the wall.
    // 0.5 = ±60° (default), 0.7 = ±45° (strict), 0.3 = ±72° (permissive).
    // Higher values require more precise alignment. 0 = any direction.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Arbitration", meta=(ClampMin="0.0", ClampMax="1.0"))
    float MinVaultInputAlignment = 0.5f;

    // --- Candidate Scoring ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Scoring", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ScoreWeightDistance = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Scoring", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ScoreWeightAngle = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Scoring", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ScoreWeightWarpDelta = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Scoring", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ScoreWeightHeight = 0.15f;

    // --- Input blending ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Physics", meta=(ClampMin="0.0", ClampMax="1.0"))
    float VelocityTraceBlend = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Physics", meta=(ClampMin="0.0"))
    float VelocityBlendMinSpeed = 50.f;

    // --- Ledge validation ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Physics", meta=(ClampMin="1", ClampMax="5"))
    int32 LedgeSampleCount = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Physics", meta=(ClampMin="0.0", ClampMax="1.0"))
    float LedgeSamplePassRatio = 0.5f;

    // --- Gravity prediction ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Physics", meta=(ClampMin="0.0", ClampMax="0.5"))
    float GravityLookaheadTime = 0.1f;

    // --- Cache ---

    // Fraction of capsule radius that invalidates the trace cache.
    // Cache is ONLY checked when CanActivateAbility is called -- no timers, no ticks.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Detection", meta=(ClampMin="0.1", ClampMax="1.0"))
    float CacheInvalidationRadiusFraction = 0.25f;

private:
    // Build detection config from this ability's UPROPERTYs.
    FOSMantleDetectionConfig BuildDetectionConfig() const;

    UAnimMontage* SelectMontage(const FOSMantleTraceResult& TraceResult, float ActualFeetZ) const;

    void SetupMotionWarping(UMotionWarpingComponent* MotionWarpComp, const FOSMantleTraceResult& TraceResult);

    // Restore character position, rotation, and movement mode to pre-mantle state.
    // Called from rejection paths in ActivateAbility and from EndAbility on cancel.
    void RestorePreMantleState(ACharacter* Character);

    UFUNCTION()
    void OnMontageCompleted();

    UFUNCTION()
    void OnMontageCancelled();

    // Movement mode saved before MOVE_Flying, restored in EndAbility
    TEnumAsByte<EMovementMode> PreMantleMovementMode = MOVE_Walking;
    bool bMovementModeChanged = false;

    // Position and rotation saved before pre-position teleport + pre-rotate.
    // Restored on rejection paths (warp delta too large, no montage) and in EndAbility
    // when cancelled externally (death, grab). On successful completion, NOT restored —
    // the montage drives the character to the final position.
    FVector PreMantlePosition = FVector::ZeroVector;
    FRotator PreMantleRotation = FRotator::ZeroRotator;
    bool bPreMantleStateSaved = false;

    // If the montage has already completed successfully, some "OnInterrupted" /
    // cancellation callbacks can still arrive during blend-out when
    // bStopWhenAbilityEnds=true. Guard against rolling back a completed mantle (#72).
    bool bMantleWorkCommitted = false;

	// When true, a cancellation ends mantle without restoring pre-mantle transform.
	// This is set by the canceller (e.g. dodge) right before calling CancelAbility.
	bool bSkipRollbackOnCancel = false;

    // Check whether the cached trace result is still valid for the given character state.
    // Used by both CanActivateAbility and ActivateAbility to avoid duplicating threshold logic.
    bool IsCacheValid(const ACharacter* Character) const;

    // --- CanActivateAbility cache (no timers, position-threshold invalidation) ---
    mutable FOSMantleTraceResult CachedTraceResult;
    mutable FVector CachedTracePosition = FVector::ZeroVector;
    mutable float CachedTraceYaw = 0.f;
    mutable bool bCachedAirborne = false;
    mutable FVector CachedInputDirection = FVector::ZeroVector;
    mutable bool bCacheValid = false;
};
