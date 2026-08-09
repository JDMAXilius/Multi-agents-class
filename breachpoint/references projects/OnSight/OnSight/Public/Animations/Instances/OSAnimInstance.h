#pragma once
// Event-driven anim instance: ASC tag delegates update cached bools on change (no per-tick polling).
// NativeThreadSafeUpdateAnimation (worker thread) handles movement math only.
// CombatState priority: Dead > Stunned > HitReacting > KnockedDown > Grabbed > Grabbing >
// Recoiling > Blocking > Charging > Attacking > Mantling > Dodging > Casting > Idle.
// GameplayTagPropertyMap remains available for designers to wire Blueprint variables via the editor.

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h"
#include "Data/OSCombatTypes.h"
#include "OSAnimInstance.generated.h"

class AOSCharacter;
class UCharacterMovementComponent;
class UAbilitySystemComponent;

UCLASS()
class ONSIGHT_API UOSAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UOSAnimInstance(const FObjectInitializer& ObjectInitializer);

	void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	// ========================================
	// CHARACTER REFERENCES
	// ========================================
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	AOSCharacter* Character;

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	UCharacterMovementComponent* MovementComponent;

	// ========================================
	// MOVEMENT DATA (raw CMC reads, worker thread)
	// ========================================
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FRotator MovementRotation = FRotator::ZeroRotator;

	// ========================================
	// JUMP / AIRBORNE DATA
	// ========================================

	/** Vertical velocity (positive = rising, negative = falling). Drives jump blendspace. */
	UPROPERTY(BlueprintReadOnly, Category = "Movement|Jump")
	float FallSpeed = 0.f;

	/** Seconds until jump apex (0 when falling or grounded). For distance-matched jump peak. */
	UPROPERTY(BlueprintReadOnly, Category = "Movement|Jump")
	float TimeToJumpApex = 0.f;

	/** Predicted distance to ground (cm). 0 when grounded. For distance-matched landings. */
	UPROPERTY(BlueprintReadOnly, Category = "Movement|Jump")
	float DistanceToGround = 0.f;

	// ========================================
	// STANCE / FOOT TRACKING
	// ========================================
	// Comprehensive stance system: per-foot data (FOSFootState) + derived stride state +
	// foot IK policy, all in one struct. Two-tier sourcing: anim curves preferred, bone fallback.

	/** Complete stance state. Access per-foot data via Stance.LeftFoot / Stance.RightFoot.
	  * Derived state: Stance.bLeftFootForward, Stance.StanceFoot, Stance.StridePhase, etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Stance")
	FOSStanceState Stance;

	/** Bone names for foot-position fallback. Override if skeleton uses non-standard names. */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance")
	FName LeftFootBoneName = FName(TEXT("foot_l"));

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance")
	FName RightFootBoneName = FName(TEXT("foot_r"));

	// --- Stance Settings ---

	/** Skeleton's forward direction in component space. UE5 Mannequin = -Y.
	  * Used to compute ForwardOffset (stride direction projection). */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance|Settings")
	FVector SkeletonForwardCS = FVector(0.f, -1.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance|Settings", meta = (ClampMin = "1"))
	float FootPlantSpeedThreshold = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance|Settings", meta = (ClampMin = "0"))
	float ForwardFootHysteresis = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance|Settings", meta = (ClampMin = "0"))
	float FootCrossingThreshold = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance|Settings", meta = (ClampMin = "1"))
	float MaxFootLiftHeight = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance|Settings", meta = (ClampMin = "10"))
	float FootTraceLength = 75.f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance|Settings")
	TEnumAsByte<ECollisionChannel> FootTraceChannel = ECC_WorldStatic;

	/** PlantWeight threshold for bLeftFootDown/bRightFootDown convenience booleans. */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance|Settings", meta = (ClampMin = "0", ClampMax = "1"))
	float PlantWeightThreshold = 0.5f;

	/** Expected half-stride duration (seconds) for Tier 2 stride phase interpolation.
	  * Walk ~0.4s, jog ~0.3s, sprint ~0.25s. Affects phase accuracy between contacts. */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance|Settings", meta = (ClampMin = "0.1"))
	float HalfStrideTime = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Stance|Settings", meta = (ClampMin = "1"))
	float FootIKBlendSpeed = 10.f;

	/** Interp speed for lean smoothing. Higher = snappier lean response. */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Lean|Settings", meta = (ClampMin = "1"))
	float LeanInterpSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Lean|Settings", meta = (ClampMin = "1"))
	float MaxTurnLeanYawSpeed = 200.f;

	// ========================================
	// LOCOMOTION DATA (derived from movement each frame, worker thread)
	// ========================================

	/** Angle between facing and movement direction. -180 to 180. 0 = forward. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float LocomotionAngle = 0.f;

	/** Cardinal direction (with hysteresis deadzone to prevent flickering). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	EOSLocomotionDirection LocomotionDirection = EOSLocomotionDirection::Forward;

	/** Previous frame's direction — used by spin state transition logic. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	EOSLocomotionDirection PreviousLocomotionDirection = EOSLocomotionDirection::Forward;

	/** Hip orientation during strafing. Drives animation set selection (hips-forward vs hips-backward). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	EOSHipFacing HipFacingDirection = EOSHipFacing::Forward;

	/** Locomotion angle adjusted for root yaw offset. Fed to Orientation Warping layers.
	  * Equals LocomotionAngle until Turn in Place adds RootYawOffset. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float LocomotionAngleWithOffset = 0.f;

	/** Velocity in character-local space (unrotated). X = forward/back, Y = left/right. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	FVector2D LocalVelocity2D = FVector2D::ZeroVector;

	/** Yaw change per second (degrees/sec). Drives turning lean blendspace. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float YawDeltaSpeed = 0.f;

	/** Acceleration in character-local space, normalized to [-1,1]. Drives accel lean blendspace. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	FVector2D RelativeAcceleration2D = FVector2D::ZeroVector;

	/** Distance traveled this frame (world units). Used by distance-matched starts. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float LocationDelta = 0.f;

	/** Cumulative distance since reset. AnimBP resets on state entry (e.g., entering Start state). */
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion")
	float AccumulatedDistance = 0.f;

	/** Current gait — Walk/Jog/Sprint. Tags win over speed threshold. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	EOSGait Gait = EOSGait::Jog;

	// ========================================
	// STRIDE WARPING DATA (for AnimNode_StrideWarping)
	// ========================================
	// StrideScale is NOT computed here — use StrideWarping node in Graph mode
	// (EWarpingEvaluationMode::Graph), which reads root motion speed from the
	// animation attribute stream and computes scale internally.
	// We provide StrideDirection (pin input) and Speed (as LocomotionSpeed pin).

	/** Component-space stride direction. Derived from LocomotionAngle.
	  * Fed to StrideWarping node StrideDirection pin. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Stride")
	FVector StrideDirection = FVector::ForwardVector;

	// ========================================
	// TURN IN PLACE (root yaw offset system)
	// ========================================

	/** Angular gap between actor rotation and mesh facing. Driven by Rotate Root Bone node.
	  * Positive = mesh rotated clockwise from actor. Range: -180 to 180. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|TurnInPlace")
	float RootYawOffset = 0.f;

	/** Previous frame's offset — for delta calculations in AnimBP turn-yaw-curve processing. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|TurnInPlace")
	float PreviousRootYawOffset = 0.f;

	/** Current offset mode. AnimBP sets this per-state (Idle=Accumulate, Stop=Hold, default=BlendOut). */
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|TurnInPlace")
	EOSRootYawOffsetMode RootYawOffsetMode = EOSRootYawOffsetMode::BlendOut;

	// --- Locomotion Settings ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Settings")
	FOSLocomotionDirectionSettings LocomotionSettings;

	/** Speed below this = Walk gait (when no tag override). Above = Jog. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Settings", meta = (ClampMin = "0"))
	float WalkSpeedThreshold = 150.f;

	/** Max downward trace distance (cm) for DistanceToGround while airborne. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Settings|Jump", meta = (ClampMin = "100"))
	float MaxGroundTraceRange = 5000.f;

	/** Spring frequency (Hz) for blending RootYawOffset back to zero. Higher = faster snapback.
	  * Critically damped (no oscillation). ~3-5 Hz feels natural for locomotion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Settings|TurnInPlace", meta = (ClampMin = "0"))
	float RootYawBlendOutSpringFrequency = 4.f;

	// ========================================
	// BODY LEAN (pre-computed for additive blendspace layers)
	// ========================================

	/** Pre-computed lean angles from acceleration and turn rate. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Lean")
	FOSBodyLeanState BodyLean;

	// ========================================
	// COMBAT DATA
	// ========================================
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	EOSCombatState CombatState = EOSCombatState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	EOSAttackPhase AttackPhase = EOSAttackPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsInCombat = false;

	// ========================================
	// GAMEPLAY TAG SYSTEM
	// ========================================
	/**
	 * Gameplay tags that can be mapped to blueprint variables.
	 * The variables will automatically update as the tags are added or removed.
	 * These should be used instead of manually querying for the gameplay tags.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	// ========================================
	// GAMEPLAY TAG BOOLEANS
	// ========================================
	// Updated via ASC tag-change delegates (RegisterGameplayTagEvent). Event-driven — no per-tick
	// polling. Delegates fire on the game thread only when a tag transitions through zero (added
	// when count 0→1, removed when count N→0). Between state changes, zero game-thread cost.
	//
	// WHY DELEGATES INSTEAD OF GameplayTagPropertyMap:
	// The property map's editor dropdown (GameplayTagBlueprintPropertyMappingDetails.cpp:62-66)
	// only lists Blueprint-created variables — C++ UPROPERTYs are excluded by design (no UBlueprint
	// owner class, no GUID for rename tracking). The runtime Initialize() path has no such filter,
	// but populating PropertyMappings from C++ is fragile (Blueprint CDO serialization can overwrite
	// constructor values, and repeated InitializeWithAbilitySystem calls would duplicate entries).
	//
	// RegisterGameplayTagEvent delegates are the cleanest event-driven approach for C++ bools.
	// The GameplayTagPropertyMap remains available for designers to wire Blueprint-created
	// variables via the editor — both systems coexist, reading the same ASC tag state.

	// --- State Tags ---
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsDead = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsStunned = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsHitReacting = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsKnockedDown = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsGrabbing = false;

	// IsGrabbed: from GA_OSGrabReaction ActivationOwnedTags (victim ability active)
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsGrabbed = false;

	// IsBeingGrabbed: from GE_OSGrabClaim (applied before reaction ability activates). Both drive Grabbed combat state.
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsBeingGrabbed = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsBlocking = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsCharging = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsAttacking = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsMantling = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsDodging = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsRecoiling = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsLockedOn = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsInAir = false;

	/** ASC: Gameplay.State.Movement.Airborne (alias in assets: State.Movement.Airborne). Same window as InAir GE. */
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_State_Movement_Airborne = false;

	/** ASC: Gameplay.State.Movement.Grounded (alias: State.Movement.Grounded). Walk / NavWalk GE. */
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_State_Movement_Grounded = false;

	/** ASC: Gameplay.State.Movement.Jumping (alias: State.Movement.Jumping). Short window after jump GA (server). */
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_State_Movement_Jumping = false;

	// Derived: !IsInAir (not a direct tag read). IsGrounded is the implicit default (absence of GE_OSInAirState).
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsGrounded = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_AttackCommitted = false;

	// --- Attack Phase Tags ---
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_State_Attack_Windup = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_State_Attack_Active = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_State_Attack_Recovery = false;

	// --- Ability Tags ---
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_Ability_Dodge = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_Ability_Jump = false;

	// --- Attribute Tags ---
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsStaminaBlocked = false;

	// --- Event Tags ---
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_Event_GuardBreak = false;

	// --- Cue Tags ---
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_Cue_Flinch = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_Cue_ClashKnockback = false;

private:
	// ========================================
	// TAG DELEGATE SYSTEM
	// ========================================
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
	TMap<FGameplayTag, bool*> TagBoolMap;
	TArray<TPair<FGameplayTag, FDelegateHandle>> TagDelegateHandles;

	void RegisterTagDelegates(UAbilitySystemComponent* ASC);
	void UnregisterTagDelegates();
	void OnGameplayTagChanged(const FGameplayTag Tag, int32 NewCount);
	void RecalculateDerivedState();

	// ========================================
	// LOCOMOTION FRAME TRACKERS
	// ========================================
	float PreviousYaw = 0.f;
	FVector PreviousWorldLocation = FVector::ZeroVector;
	FVector2D PreviousVelocity2D = FVector2D::ZeroVector;
	float RootYawSpringVelocity = 0.f; // Internal state for FFloatSpringState

	// Foot stance tracking state
	bool bWasAccelerating = false;
	float LastMovingPlantDelta = 0.f;
	FVector PreviousLeftFootLocationCS = FVector::ZeroVector;
	FVector PreviousRightFootLocationCS = FVector::ZeroVector;
	bool bPreviousLeftFootDown = true;
	bool bPreviousRightFootDown = false;
	float TimeSinceLastContact = 0.f;
	float LastContactPhase = 0.f;
	float DynamicHalfStrideTime = 0.3f;  // Adapts to actual contact intervals

	// Cached per-foot ground trace results (written on game thread, read on worker thread)
	FHitResult CachedLeftGroundHit;
	FHitResult CachedRightGroundHit;
	bool bCachedLeftGroundValid = false;
	bool bCachedRightGroundValid = false;

	// Cached bone indices (resolved once in NativeInitializeAnimation)
	int32 LeftFootBoneIndex = INDEX_NONE;
	int32 RightFootBoneIndex = INDEX_NONE;
	int32 LeftKneeBoneIndex = INDEX_NONE;
	int32 RightKneeBoneIndex = INDEX_NONE;

	// Normalized skeleton forward (cached from SkeletonForwardCS on init)
	FVector CachedSkeletonForwardCS = FVector(0.f, -1.f, 0.f);

	// ========================================
	// HELPER FUNCTIONS
	// ========================================
	void UpdateMovementData();
	void UpdateFootGroundTraces();           // Game thread — per-foot line traces
	void UpdateStance(float DeltaSeconds);   // Worker thread — bone reads + derived state
	void ComputeFootState(FOSFootState& OutFoot, int32 FootBoneIndex, int32 KneeBoneIndex,
		float PlantCurve, bool bPlantCurvesActive, float SpeedCurve,
		const FHitResult& CachedGroundHit, bool bGroundHitValid,
		FVector& InOutPreviousLocationCS,
		const TArray<FTransform>& CSTransforms, const FVector& RootLocationCS,
		const FTransform& ComponentToWorld, float DeltaSeconds);
	void ComputeDerivedStanceState(float DeltaSeconds);
	void UpdateStride();
	void UpdateBodyLean(float DeltaSeconds);
	void ComputeFootIKPolicy(float DeltaSeconds);
	void UpdateLocomotionData(float DeltaSeconds);
	void UpdateRootYawOffset(float DeltaSeconds, float ActorYawDelta);
	void SetRootYawOffset(float NewOffset);
	void ComputeRelativeAcceleration(float DeltaSeconds);
	EOSLocomotionDirection CalculateLocomotionDirection(float Angle) const;
	EOSHipFacing CalculateHipFacing(EOSLocomotionDirection NewDir, EOSLocomotionDirection PrevDir) const;
	EOSCombatState EvaluateCombatState() const;
	EOSAttackPhase EvaluateAttackPhase() const;
};
