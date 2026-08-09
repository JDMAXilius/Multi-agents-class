#pragma once
// Base for all attack abilities: soft auto-targeting, motion warping toward target, and shared target/warp API.
// Children: GA_OSComboAttack (GA_OSChargedAttack is deprecated — extends UOSGameplayAbility directly)

#include "CoreMinimal.h"
#include "Data/OSCombatTypes.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "Interfaces/OSAbilityDistanceInterface.h"
#include "GA_OSAttack.generated.h"

class AActor;
class ACharacter;
class UAnimMontage;
class UMotionWarpingComponent;
class UAbilityTask_WaitGameplayEvent;

/**
 * Base gameplay ability for attacks. Provides optional soft-targeting, motion warping toward target,
 * and a single current-target + warp-target API. Subclasses implement combo, charged, or single-hit flow.
 * Sets shared ActivationBlockedTags (mantling, CC states, grabs, stun-lock); UOSGameplayAbility adds death.
 */
UCLASS(Abstract)
class ONSIGHT_API UGA_OSAttack : public UOSGameplayAbility, public IOSAbilityDistanceInterface
{
	GENERATED_BODY()

public:
	UGA_OSAttack();

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	float GetSoftTargetConeAngle() const { return SoftTargetConeAngle; }
	float GetOptimalMeleeDistance() const { return OptimalMeleeDistance; }

	/** Combo-chain subclasses require persistent MW targets across hits; see base declaration. */
	virtual bool PersistsWarpTargetsAcrossNotifyEnd() const override { return true; }

#if !UE_BUILD_SHIPPING
	FVector DebugWarpPathStart = FVector::ZeroVector;
	FVector DebugWarpPathEnd = FVector::ZeroVector;
	FVector DebugLastTrackedLoc = FVector::ZeroVector;
	TArray<FTransform> DebugOrientationGhosts;
	float DebugGhostTimer = 0.f;
#endif

protected:
	// ========================================
	// SOFT TARGETING
	// ========================================

	/** Enable soft-targeting: find enemy in cone and warp toward them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|Attack|Soft Target")
	bool bSoftTargeting = false;

	/** Half-angle of the cone search (degrees). 90 = 180-degree hemisphere in front of aim. */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (ClampMin = "10", ClampMax = "360", UIMin = "10", UIMax = "360", EditCondition = "bSoftTargeting"))
	float SoftTargetConeAngle = 90.f;

	/** Maximum distance for soft-target acquisition. */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (ClampMin = "50", UIMin = "50", EditCondition = "bSoftTargeting"))
	float SoftTargetMaxRange = 500.f;

	/** Tags that exclude characters from soft-targeting (e.g. IsDead, IsStunned, IsGrabbed). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (EditCondition = "bSoftTargeting"))
	FGameplayTagContainer SoftTargetExcludeTags;

	/** Which blending algorithm to use when combining target direction with input during attacks.
	 *  LinearLerp: Simple angular interpolation. Predictable, easy to tune.
	 *  Magnetic: Snaps to target until input pushes past a breakaway angle. Fighting-game feel.
	 *  WeightedAngle: Dead zone near target, smooth ramp to full input control. Best for precision.
	 *  ConeClamp: Input steers freely within a cone around the target direction. Action-game feel. */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (EditCondition = "bSoftTargeting"))
	EOSBlendAlgorithm BlendAlgorithm = EOSBlendAlgorithm::Magnetic;

	// --- Algorithm A: Linear Lerp ---

	/** Fraction of the angle between target and input to rotate toward input.
	 *  0 = locked on target, 1 = pure input direction.
	 *  Presets: 0.1 (strong lock-on), 0.25 (balanced), 0.5 (loose assist), 0.8 (near-free aim). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", EditCondition = "bSoftTargeting && BlendAlgorithm==EOSBlendAlgorithm::LinearLerp", EditConditionHides))
	float SoftTargetInputInfluence = 0.15f;

	// --- Algorithm B: Magnetic ---

	/** How strongly the character sticks to the target direction. Higher = harder to pull away.
	 *  Presets: 0.3 (light magnetism), 0.6 (moderate, Sifu-like), 0.85 (heavy, For Honor-like). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", EditCondition = "bSoftTargeting && BlendAlgorithm==EOSBlendAlgorithm::Magnetic", EditConditionHides))
	float MagneticStickiness = 0.7f;

	/** Input angle (degrees) that breaks magnetic lock and lets the character redirect.
	 *  Below this angle, stickiness dominates. Above it, input takes over.
	 *  Presets: 30 (tight lock), 60 (balanced), 120 (only breaks on full reversal). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (ClampMin = "5", ClampMax = "180", UIMin = "5", UIMax = "180", EditCondition = "bSoftTargeting && BlendAlgorithm==EOSBlendAlgorithm::Magnetic", EditConditionHides))
	float MagneticBreakawayAngle = 60.f;

	// --- Algorithm C: Weighted Angle ---

	/** Input within this angle of the target is ignored (pure lock-on zone, degrees).
	 *  Presets: 5 (minimal dead zone), 15 (moderate), 30 (wide lock-on). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (ClampMin = "0", ClampMax = "90", UIMin = "0", UIMax = "90", EditCondition = "bSoftTargeting && BlendAlgorithm==EOSBlendAlgorithm::WeightedAngle", EditConditionHides))
	float DeadZoneAngle = 15.f;

	/** At this angle from target, input has full control (degrees). Between DeadZone and this, blends smoothly.
	 *  Presets: 45 (responsive), 90 (balanced ramp), 135 (gradual transition). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (ClampMin = "10", ClampMax = "180", UIMin = "10", UIMax = "180", EditCondition = "bSoftTargeting && BlendAlgorithm==EOSBlendAlgorithm::WeightedAngle", EditConditionHides))
	float FullControlAngle = 90.f;

	/** Curve shape for the dead-zone-to-full-control ramp. 1 = linear, 2 = ease-in, 3+ = snappy transition.
	 *  Presets: 1.0 (linear), 2.0 (smooth ease-in), 3.0 (sharp snap to full control). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (ClampMin = "0.1", ClampMax = "5", UIMin = "0.1", UIMax = "5", EditCondition = "bSoftTargeting && BlendAlgorithm==EOSBlendAlgorithm::WeightedAngle", EditConditionHides))
	float BlendCurveExponent = 2.f;

	// --- Algorithm D: Cone Clamp ---

	/** Maximum angle (degrees) the character can deviate from the target direction via input.
	 *  Input is free within this cone but clamped at the edge.
	 *  Presets: 20 (tight cone), 45 (balanced), 90 (wide freedom). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (ClampMin = "5", ClampMax = "180", UIMin = "5", UIMax = "180", EditCondition = "bSoftTargeting && BlendAlgorithm==EOSBlendAlgorithm::ConeClamp", EditConditionHides))
	float MaxDeviationAngle = 45.f;

	/** How much input affects direction within the cone. 0 = locked on target, 1 = full input steering.
	 *  Presets: 0.5 (heavy assist), 0.8 (light assist), 1.0 (pure input within cone). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Soft Target", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", EditCondition = "bSoftTargeting && BlendAlgorithm==EOSBlendAlgorithm::ConeClamp", EditConditionHides))
	float ConeInputWeight = 1.f;

	// ========================================
	// MOTION WARPING
	// ========================================

	/** Warp target name used for all MW injection (position + rotation).
	 *  Must match the notify's fallback WarpTargetName on attack montages. */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Motion Warp")
	FName WarpTargetName = "AttackTarget";

	/** Optimal melee distance (cm). Standoff offset — character stops this far from target. */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Motion Warp", meta = (ClampMin = "0", UIMin = "0"))
	float OptimalMeleeDistance = 90.f;

	/** How far the character lunges forward when no target is found (directional air-swing, cm). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Motion Warp", meta = (ClampMin = "0", UIMin = "0"))
	float DirectionalFallbackWarpDistance = 100.f;

	/** When true, the notify tick refreshes warp POSITION every frame (tracks moving target).
	 *  When false, lunge destination is locked at injection time. */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Motion Warp")
	bool bRefreshWarpPosition = false;

	/** When true, the notify tick refreshes warp ROTATION every frame (re-blends input direction).
	 *  When false, facing direction is locked at injection time. */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Motion Warp")
	bool bRefreshWarpRotation = false;

	/** Maximum degrees per second the warp rotation can change during per-tick refresh.
	 *  Controls how quickly the character can redirect mid-attack. Lower = more magnetic/committed.
	 *  0 = unlimited (blend algorithm output applied instantly).
	 *  Presets: 90 (heavy commitment), 180 (responsive), 360 (fast redirect), 0 (instant). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Attack|Motion Warp", meta = (ClampMin = "0", UIMin = "0", EditCondition = "bRefreshWarpRotation"))
	float WarpRotationSpeedDegPerSec = 180.f;

	// ========================================
	// TARGET RESOLUTION & WARP INJECTION
	// ========================================

	/** Soft-target resolution: cone search + direction blend. Stores Target and StoredBlendedDirection2D. */
	virtual void ResolveAttackTarget(UAnimMontage* Montage = nullptr);

	/** Inject warp targets on the MWComponent.
	 *  Called after ResolveAttackTarget, before montage plays. */
	void InjectWarpTargets(ACharacter* Character);

	/** Clear warp target on EndAbility. */
	void ClearWarpTarget(ACharacter* Character) const;

public:
	// --- Getters for notify and tasks to read GA config (single source of truth) ---

	/** Dispatch blend algorithm on BaseDir vs InputDir using current soft-target params.
	 *  Public: called by RefreshWarpTarget for per-frame rotation blending. */
	FVector BlendDirectionForWarp(const FVector& BaseDir, const FVector& InputDir) const;

	/** Per-frame refresh of warp target transform (position tracks target, rotation re-blends input).
	 *  Called from the motion warping notify's tick — runs every frame during the warp window.
	 *  Only active when bRefreshWarpPosition or bRefreshWarpRotation is true. */
	void RefreshWarpTarget(ACharacter* Character, float DeltaTime);

	bool HasSoftTargeting() const { return bSoftTargeting; }
	bool WantsPerTickWarpRefresh() const { return bRefreshWarpPosition || bRefreshWarpRotation; }

	UFUNCTION(BlueprintPure, Category = "OnSight|Attack")
	AActor* GetCurrentSoftTarget() const { return Target.Get(); }

	FVector GetStoredBlendedDirection2D() const { return StoredBlendedDirection2D; }

	UFUNCTION(BlueprintPure, Category = "OnSight|Attack")
	virtual FName GetWarpTargetName() const;

	/** Per-hit warp index appended to WarpTargetName to produce unique names per hit
	 *  (e.g. "AttackTarget_0", "AttackTarget_2"). Prevents stale modifiers from reading the
	 *  wrong hit's target during combo transitions. Base returns 0 (single-hit).
	 *  GA_OSComboAttack overrides to return CurrentComboIndex — single source of truth. */
	virtual int32 GetWarpHitIndex() const { return 0; }

	float GetSoftTargetMaxRange() const { return SoftTargetMaxRange; }
	float GetDirectionalFallbackWarpDistance() const { return DirectionalFallbackWarpDistance; }

	void FillSoftTargetParams(FOSAttackDirectionParams& OutParams) const;

	/** Current soft-lock target. Set by ResolveAttackTarget (cone result); used by notifies and hit traces.
	  * Weak ref: target may be destroyed mid-ability (disconnect, death). Callers must check IsValid(). */
	TWeakObjectPtr<AActor> Target;

	/** Trigger event data (e.g. for passing target to hit notifies). Subclasses may set Target on this. */
	FGameplayEventData StoredTriggerEventData;

	// IOSAbilityDistanceInterface
	virtual bool HasMeleeOptimalDistance() const override;
	virtual float GetMeleeOptimalDistance() const override;

	/** Server → owning client: sync warp state after InjectWarpTargets runs on the authority.
	 *  Includes hit index so the client can reject stale corrections that arrive after it has
	 *  already advanced to the next combo hit (out-of-order RPCs under latency).
	 *  Reliable because this data drives the warp destination — a missed sync causes position divergence. */
	UFUNCTION(Client, Reliable)
	void ClientRPC_SyncWarpState(const FVector& InBlendedDirection, bool bInHasTarget, int32 InWarpHitIndex);


protected:
	/** Blended direction from last ResolveAttackTarget. Used by InjectWarpTargets and
	 *  RefreshWarpTarget. Synced to client via ClientRPC_SyncWarpState after server injection. */
	FVector StoredBlendedDirection2D = FVector::ZeroVector;

	/** Whether the current warp was injected with a valid target. Locked at injection time.
	 *  Synced to client via ClientRPC_SyncWarpState after server injection. */
	bool bWarpHasTarget = false;

	/** Position-only: recompute lunge destination from live target location. Target must be valid. */
	void ComputeWarpPositionWithTarget(const ACharacter* Character, const FVector& OwnerLoc, FVector& OutWarpLoc) const;

	/** Rotation-only: recompute facing direction from stored blend direction + live input.
	 *  Returns false if no valid base direction could be resolved. */
	bool ComputeWarpRotation(const ACharacter* Character, const FVector& InputDir, FVector& OutFaceDir) const;

	/** Apply temporal damping to limit rotation speed per frame. Returns damped face direction. */
	FVector ApplyRotationDamping(const UMotionWarpingComponent* MWC, const FVector& DesiredDir, float DeltaTime) const;

	/** Cached MWC reference — populated in InjectWarpTargets, used in RefreshWarpTarget
	 *  to avoid per-frame FindComponentByClass. Cleared on EndAbility/OnRemoveAbility. */
	TWeakObjectPtr<UMotionWarpingComponent> CachedMWC;

	// ========================================
	// ATTACK PHASE TRACKING
	// ========================================

	/** Current attack phase. Managed exclusively by SetPhase(). */
	EOSAttackPhase CurrentPhase = EOSAttackPhase::None;

	/** Transition to a new phase. Sets the replicated loose gameplay tag for the new phase
	 *  and removes the old phase's tag via SetReplicatedLooseGameplayTagCount.
	 *  Calls OnPhaseChanged() after the transition completes. */
	void SetPhase(EOSAttackPhase NewPhase);

	/** Override in subclasses for additional behavior on phase transitions
	 *  (e.g. start traces on Active, resolve combo on Recovery). */
	virtual void OnPhaseChanged(EOSAttackPhase OldPhase, EOSAttackPhase NewPhase) {}

	/** Create WaitGameplayEvent tasks for phase events and set initial Windup phase.
	 *  Call at the start of each attack hit (PerformComboHit, single-hit activate, etc.).
	 *  Tasks persist across combo section jumps. */
	void StartPhaseTracking();

	/** Remove active phase tag and reset CurrentPhase to None. Called by EndAbility. */
	void ClearPhaseState();

private:
	UFUNCTION() void OnPhaseActiveEvent(FGameplayEventData Payload);
	UFUNCTION() void OnPhaseRecoveryEvent(FGameplayEventData Payload);

	UPROPERTY() TObjectPtr<UAbilityTask_WaitGameplayEvent> PhaseActiveTask;
	UPROPERTY() TObjectPtr<UAbilityTask_WaitGameplayEvent> PhaseRecoveryTask;
};
