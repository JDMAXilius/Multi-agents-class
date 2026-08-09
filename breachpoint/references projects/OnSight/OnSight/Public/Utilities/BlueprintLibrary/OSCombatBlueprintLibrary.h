#pragma once
// Combat utilities: geometry, target acquisition, damage application, motion warping helpers.

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "Data/OSCombatTypes.h"
#include "Data/OSDefenseAndReactions.h"
#include "RootMotionModifier.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OSCombatBlueprintLibrary.generated.h"

class AOSCharacter;
class ACharacter;
class UAnimMontage;
class UMotionWarpingComponent;
class USceneComponent;
enum class EOSAttackType : uint8;
class UGameplayEffect;

UCLASS()
class ONSIGHT_API UOSCombatBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========================================
	// TARGET ACQUISITION
	// ========================================

	/** Server-authoritative box trace forward from actor.
	 *  Returns first valid OSCharacter hit (ignores self).
	 *  Rejects targets whose ASC has any tag in ExcludeTags.
	 *  Call only on HasAuthority(). */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat", meta=(DefaultToSelf="Instigator"))
	static AOSCharacter* BoxTraceForTarget(
		const AActor* Instigator,
		const FGameplayTagContainer& ExcludeTags,
		float HalfExtent = 50.f,
		float ForwardDistance = 100.f);

	/** Get the nearest OSCharacter within range, excluding characters with any ExcludeTag.
	 *  Unlike BoxTraceForTarget, uses sphere overlap (no directionality).
	 *  DEPRECATED: Use FindBestScoredTarget for scored selection with LOS, or FindNearestTargetInCone for directional search. */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat", meta=(DeprecatedFunction, DeprecationMessage="Use FindBestScoredTarget or FindBestTargetInCone instead."))
	static AOSCharacter* FindNearestTarget(
		const AActor* Origin,
		const FGameplayTagContainer& ExcludeTags,
		float Radius = 500.f);

	/** Find best-scored OSCharacter within a cone defined by Origin + Direction.
	 *  Uses sphere overlap (ECC_Pawn), filters by dot-product angle + line-of-sight,
	 *  then scores candidates by distance (1x) + angle alignment (2x).
	 *  ExcludeTags: skip characters whose ASC has any of these tags.
	 *  Returns nullptr if no valid target in cone or Direction2D is zero. */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat")
	static AOSCharacter* FindBestTargetInCone(
		const AActor* Origin,
		const FVector& Direction2D,
		const FGameplayTagContainer& ExcludeTags,
		float ConeAngleDegrees = 90.f,
		float Radius = 500.f);


	/** Gather all valid OSCharacters within radius, excluding characters with any ExcludeTag.
	 *  Returns unsorted array of candidates. Foundation for scored/filtered target selection. */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat")
	static TArray<AOSCharacter*> GatherTargetCandidates(
		const AActor* Origin,
		const FGameplayTagContainer& ExcludeTags,
		float Radius = 500.f);

	/** Line trace from Origin to Target, both at actor location + HeightOffset.
	 *  Returns true if nothing blocks the trace (ignores Origin and Target actors).
	 *  Uses ECC_Visibility channel. HeightOffset default 90 = chest height.
	 *  Server-safe: uses actor locations, not camera viewpoint. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static bool HasLineOfSightToTarget(
		const AActor* Origin,
		const AActor* Target,
		float HeightOffset = 90.f);

	/** Compute weighted targeting score for a candidate.
	 *  DistanceScore = 1.0 - (Distance / MaxRange), clamped [0,1]. Weight: DistanceWeight.
	 *  AngleScore = DotProduct(ReferenceDir2D, ToCandidate2D), clamped [0,1]. Weight: AngleWeight.
	 *  Returns DistanceScore * DistanceWeight + AngleScore * AngleWeight.
	 *  Higher is better. Returns 0 if candidate is beyond MaxRange. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static float ScoreTargetCandidate(
		const AActor* Origin,
		const FVector& ReferenceDirection2D,
		const AActor* Candidate,
		float MaxRange,
		float DistanceWeight = 1.f,
		float AngleWeight = 2.f);

	/** Full targeting pipeline: gather candidates → optional LOS filter → score → return best.
	 *  ReferenceDirection2D: typically camera forward (GetCameraForwardDirection2D).
	 *  Returns nullptr if no valid candidate scores above 0.
	 *  bRequireLOS: when true, candidates failing line-of-sight check are rejected. */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat")
	static AOSCharacter* FindBestScoredTarget(
		const AActor* Origin,
		const FVector& ReferenceDirection2D,
		const FGameplayTagContainer& ExcludeTags,
		float MaxRange = 500.f,
		float DistanceWeight = 1.f,
		float AngleWeight = 2.f,
		bool bRequireLOS = true,
		float LOSHeightOffset = 90.f);

	/** Camera-viewpoint soft target: sphere overlap + arc + LOS + distance/angle scoring.
	 *  Uses player camera viewpoint as cone center (what the player is looking at).
	 *  Primary soft-targeting function — used by combat and MW fallback injection.
	 *  @param SoftLockArcDeg  Full cone angle in degrees (default 120 = 60° half-angle).
	 *  @param bDrawDebug  Enable debug visualization (non-shipping only). */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat")
	static AActor* FindBestSoftTarget(
		const AActor* Origin,
		float MaxRange = 1200.f,
		float SoftLockArcDeg = 120.f,
		bool bDrawDebug = false);

	/** Check if an actor is a valid combat target (not null, not self, is pawn, not dead via ASC tag). */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static bool IsValidCombatTarget(const AActor* Origin, const AActor* Candidate);

	// ========================================
	// DIRECTION & GEOMETRY
	// ========================================

	/** Compute procedural grab placement: positions Victim along the Attacker->Victim axis,
	 *  face-to-face, at the specified center-to-center distance.
	 *  CenterDistance=0 matches the old socket SnapToTarget behavior (full capsule overlap).
	 *  Returns world-space transform (location on ground plane + rotation facing attacker). */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat")
	static FTransform ComputeGrabPlacement(
		const AOSCharacter* Attacker,
		const AOSCharacter* Victim,
		float CenterDistance = 0.f);

	/** Compute normalized 2D approach direction from one actor to another (Z zeroed). */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FVector ComputeApproachDirection2D(const AActor* From, const AActor* To);

	/** 4-way direction from impact point relative to target's facing.
	 *  Uses FrontBackBias=1.15 for wider side cones (side hits don't collapse to front/back). */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static EOSDirection ComputeDirection4Way(const AActor* Target, const FVector& ImpactPoint);

	/** 4-way direction from FHitResult (convenience overload). */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat", meta=(DisplayName="Compute Direction 4-Way (HitResult)"))
	static EOSDirection ComputeDirection4WayFromHit(const AActor* Target, const FHitResult& Hit);

	/** True if Other is within AngleDegrees cone in front of Self. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static bool IsActorWithinAngle(const AActor* Origin, const AActor* Other, float AngleDegrees);

	/** Compute yaw from a world direction vector (Z ignored). Returns 0 if direction is nearly zero. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static float YawFromDirection2D(const FVector& Direction);

	/** Get camera forward direction projected to 2D (Z zeroed, normalized).
	 *  Uses GetControlRotation().Yaw — available on server via client movement RPCs.
	 *  Returns actor forward (Z zeroed) if no controller. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FVector GetCameraForwardDirection2D(const ACharacter* Character);

	/** Resolve aim direction using priority chain: input > camera forward > actor forward.
	 *  Always returns a valid normalized 2D vector. Server-safe. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FVector GetAimDirection2D(const ACharacter* Character);

	/** Last movement direction the player was traveling before the current action.
	 *  Uses CMC's LastInputVector (last applied movement input), falling back to
	 *  camera forward > actor forward. Captures intent at the moment of commitment,
	 *  not the live stick which may have already changed. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FVector GetLastMovementDirection2D(const ACharacter* Character);

	/** Compute attack direction by blending camera facing with stick input (GoW-style).
	 *  With stick input: 60% camera forward + 40% stick direction (prevents wild off-angle swings).
	 *  Without stick input: pure camera forward (attack where you're looking).
	 *  Returns yaw-only rotator (pitch/roll zeroed). Server-safe via GetControlRotation. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FRotator ComputeAttackDirection(const ACharacter* Character);

	/** Blend between a target direction and an input direction.
	 *  InputInfluence: 0.0 = pure TargetDir, 1.0 = pure InputDir.
	 *  Returns TargetDir unmodified if InputDir is nearly zero or InputInfluence <= 0.
	 *  Result is normalized 2D (Z zeroed). */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FVector BlendDirectionWithInput(
		const FVector& TargetDir,
		const FVector& InputDir,
		float InputInfluence);

	/** Magnetic blend: target pulls character facing with spring-like resistance.
	 *  Input can nudge left/right of target up to BreakawayAngle degrees.
	 *  Beyond breakaway, input takes full control (smooth 30-degree transition zone).
	 *  Stickiness: 0.0 = no magnetism (pure input), 1.0 = locked on target.
	 *  Returns TargetDir if InputDir is nearly zero. Result normalized 2D. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FVector BlendDirectionMagnetic(
		const FVector& TargetDir,
		const FVector& InputDir,
		float Stickiness = 0.7f,
		float BreakawayAngleDegrees = 60.f);

	/** Angle-weighted blend: dampens small deviations, respects large ones.
	 *  DeadZoneAngle: degrees within which input has zero effect (pure target).
	 *  FullControlAngle: degrees beyond which input has full control.
	 *  CurveExponent: 1.0 = linear ramp, 2.0 = quadratic (stickier), 0.5 = sqrt (responsive).
	 *  Returns TargetDir if InputDir is nearly zero. Result normalized 2D. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FVector BlendDirectionWeightedAngle(
		const FVector& TargetDir,
		const FVector& InputDir,
		float DeadZoneAngleDegrees = 15.f,
		float FullControlAngleDegrees = 90.f,
		float CurveExponent = 2.f);

	/** Cone-clamped blend: input moves facing freely within MaxDeviationAngle of target.
	 *  Outside the cone, result is clamped to the nearest edge.
	 *  InputWeight: how much input affects facing within the cone (0=locked, 1=free within cone).
	 *  Returns TargetDir if InputDir is nearly zero. Result normalized 2D. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FVector BlendDirectionConeClamp(
		const FVector& TargetDir,
		const FVector& InputDir,
		float MaxDeviationAngleDegrees = 45.f,
		float InputWeight = 1.f);

	/** Dispatch the blend algorithm selected in Params against BaseDir and InputDir.
	 *  Pure switch — no target search, no normalization. Returns BaseDir on unknown algorithm. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FVector DispatchBlendAlgorithm(
		const FVector& BaseDir,
		const FVector& InputDir,
		const FOSAttackDirectionParams& Params);

	/** Resolve attack direction: cone search from aim, blend target direction with input using Params.
	 *  Returns true if a target was found (OutResolvedTarget set, OutFinalDir2D blended); false if no target (OutFinalDir2D = aim dir, OutResolvedTarget = nullptr).
	 *  Caller applies rotation warp or CMC alignment to OutFinalDir2D. */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat")
	static bool ResolveAttackDirectionInCone(
		AOSCharacter* Character,
		const FOSAttackDirectionParams& Params,
		FVector& OutFinalDir2D,
		AOSCharacter*& OutResolvedTarget);

	// ========================================
	// POSITION WARP PLACEMENT
	// ========================================

	/** Minimum stop distance between two characters (sum of scaled capsule radii).
	 *  Use as the floor for any warp/placement computation. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static float GetCapsuleStopDistance(const ACharacter* A, const ACharacter* B);

	/** Sweep-test a warp path for blocking geometry.
	 *  Capsule sweep from Start along Direction for Distance units.
	 *  Returns the safe travel distance (shortened if blocked).
	 *  Uses ECC_Pawn channel, ignores Instigator and optional Target. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static float SweepWarpPath(
		const ACharacter* Instigator,
		const FVector& Start,
		const FVector& Direction,
		float Distance,
		const AActor* TargetToIgnore = nullptr);

	/** Blind-path sweep: capsule sweep from Start along Direction for Distance units.
	 *  Does NOT ignore any target — use for untargeted directional warps (air-swings, dashes).
	 *  Returns safe travel distance (shortened if blocked). */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static float SweepBlindWarpPath(
		const ACharacter* Instigator,
		const FVector& Start,
		const FVector& Direction,
		float Distance);

	/** Capsule sweep checking both ECC_Pawn and ECC_WorldStatic.
	 *  Returns the shorter safe distance of the two channels.
	 *  Use instead of SweepWarpPath when world geometry blocking matters. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static float SweepWarpPathMultiChannel(
		const ACharacter* Instigator,
		const FVector& Start,
		const FVector& Direction,
		float Distance,
		const AActor* TargetToIgnore = nullptr);

	/** Blind multi-channel sweep: no target to ignore.
	 *  For untargeted directional warps (dashes, air-swings) with world geometry blocking. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static float SweepBlindWarpPathMultiChannel(
		const ACharacter* Instigator,
		const FVector& Start,
		const FVector& Direction,
		float Distance);

	// ========================================
	// ROOT MOTION ANALYSIS
	// ========================================

	/** Get the absolute start and end time of a montage section.
	 *  Returns false if SectionName is invalid. */
	UFUNCTION(BlueprintPure, Category="OnSight|MotionWarping")
	static bool GetSectionTimeRange(
		const UAnimMontage* Montage,
		FName SectionName,
		float& OutStartTime,
		float& OutEndTime);

	/** Extract total root motion delta over a montage section.
	 *  Uses ExtractRootMotionFromTrackRange (UE 5.6 FAnimExtractContext overload).
	 *  Returns identity transform if section is invalid or has no root motion. */
	UFUNCTION(BlueprintPure, Category="OnSight|MotionWarping")
	static FTransform ExtractSectionRootMotion(
		const UAnimMontage* Montage,
		FName SectionName);

	/** Project a section's root motion onto a world-space forward axis.
	 *  Returns the signed distance the character travels along ForwardDir.
	 *  Positive = forward lunge, negative = retreat. Zero if no root motion. */
	UFUNCTION(BlueprintPure, Category="OnSight|MotionWarping")
	static float GetSectionForwardDistance(
		const UAnimMontage* Montage,
		FName SectionName,
		const FVector& ForwardDir);

	/** Get the time range of a MotionWarping notify state matching WarpTargetName.
	 *  Searches OSTrackMotionWarpTarget and engine UAnimNotifyState_MotionWarping notifies.
	 *  Returns false if no matching warp window found. */
	UFUNCTION(BlueprintPure, Category="OnSight|MotionWarping")
	static bool GetWarpWindowTimeRange(
		const UAnimMontage* Montage,
		FName WarpTargetName,
		float& OutStartTime,
		float& OutEndTime);

	/** Extract root motion during a specific warp window (by target name).
	 *  Returns identity if no matching window or no root motion. */
	UFUNCTION(BlueprintPure, Category="OnSight|MotionWarping")
	static FTransform ExtractWarpWindowRootMotion(
		const UAnimMontage* Montage,
		FName WarpTargetName);

	
	UFUNCTION(BlueprintPure, Category="OnSight|RootMotion")
	static bool HasAnyRootMotion(const UAnimInstance* AnimInstance);
	
	
	// ========================================
	// DAMAGE APPLICATION
	// ========================================

	/** Team-aware friendly check. Returns true if A and B are on the same team.
	 *  Returns false (= allow interaction) when: either is null, self-damage,
	 *  either has NoTeam (FFA mode), or different teams.
	 *  CRITICAL: NoTeam guard prevents FFA regression — without it, all FFA players
	 *  are NoTeam (ID 255), same-ID → Friendly, and nobody can damage anyone. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static bool AreActorsFriendly(const AActor* A, const AActor* B);

	/** Apply damage from a known instigator to a known victim without tracing.
	 *  Populates FOSGameplayEffectContext with attack type, hit location, PlayerState UniqueIds.
	 *  Uses SetByCaller(Data_Damage) for magnitude. GAS-compliant pipeline. */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat", meta=(DefaultToSelf="Instigator"))
	static bool ApplyDirectDamage(
		AOSCharacter* Instigator, AOSCharacter* Victim,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float Damage, EOSAttackType AttackType);

	/** Server-side trace hit damage: team + invuln gates, FOS context (hit result, PS ids, cue socket),
	 *  SetByCaller(Data_Damage), optional Event_AttackHit on target ASC. Not exposed to Blueprint (FHitResult). */
	static bool ApplyDamageFromHitResult(
		AOSCharacter* Instigator,
		AActor* HitActor,
		const FHitResult& HitResult,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float Damage,
		EOSAttackType AttackType,
		FName ImpactCueSocketName = NAME_None,
		bool bDispatchAttackHitEvent = true);

	// ========================================
	// MOTION WARPING
	// ========================================

	/** Get MotionWarpingComponent from character (nullptr if missing). Uses AOSCharacter accessor. */
	UFUNCTION(BlueprintPure, Category="OnSight|MotionWarping")
	static UMotionWarpingComponent* GetMotionWarpComponent(const AOSCharacter* Character);

	/** Set a rotation-only warp target. Returns false if character lacks MW component. */
	UFUNCTION(BlueprintCallable, Category="OnSight|MotionWarping")
	static bool SetupRotationWarp(AOSCharacter* Character, FName TargetName, float TargetYaw);

	/** Set a full-transform warp target (position + rotation). Returns false if no MW component. */
	UFUNCTION(BlueprintCallable, Category="OnSight|MotionWarping")
	static bool SetupPositionWarp(AOSCharacter* Character, FName TargetName, const FTransform& Target);

	/** Remove a named warp target. Safe to call if character lacks MW component. */
	UFUNCTION(BlueprintCallable, Category="OnSight|MotionWarping")
	static void ClearWarpTarget(AOSCharacter* Character, FName TargetName);

	/** Set a component-tracking warp target on Character's MWC.
	 *  bFollowComponent = true: engine auto-reads bone transform each frame during warp window.
	 *  LocationOffset along OffsetDirection lets you stop at arm's reach from the bone.
	 *  Returns false if character lacks MW component. */
	UFUNCTION(BlueprintCallable, Category="OnSight|MotionWarping")
	static bool SetupComponentWarp(
		ACharacter* Character,
		FName WarpTargetName,
		const USceneComponent* TargetComponent,
		FName BoneName = NAME_None,
		bool bFollowComponent = true,
		FVector LocationOffset = FVector::ZeroVector,
		FRotator RotationOffset = FRotator::ZeroRotator,
		EWarpTargetLocationOffsetDirection OffsetDirection = EWarpTargetLocationOffsetDirection::VectorFromTargetToOwner);

	/** Check if a montage contains a UAnimNotifyState_MotionWarping window
	 *  whose RootMotionModifier has a matching WarpTargetName.
	 *  Use this to decide whether MW will actually fire before skipping fallbacks. */
	UFUNCTION(BlueprintPure, Category="OnSight|MotionWarping")
	static bool MontageHasWarpWindow(const UAnimMontage* Montage, FName WarpTargetName);

	/** Collect all warp target names from a montage's MotionWarping notify states.
	 *  Useful for editor validation and debug logging. */
	UFUNCTION(BlueprintPure, Category="OnSight|MotionWarping")
	static TArray<FName> GetWarpWindowNames(const UAnimMontage* Montage);

	/** Count the number of UAnimNotifyState_MotionWarping windows on a montage. */
	UFUNCTION(BlueprintPure, Category="OnSight|MotionWarping")
	static int32 CountWarpWindows(const UAnimMontage* Montage);

	/** Check if montage has an OSAnimNotifyState_OSTrackMotionWarpTarget window
	 *  matching the given WarpTargetName. */
	UFUNCTION(BlueprintPure, Category="OnSight|MotionWarping")
	static bool MontageHasOSWarpNotify(const UAnimMontage* Montage,
		FName WarpTargetName = "AttackTarget");

	// ========================================
	// CHARACTER ALIGNMENT UTILITIES
	// ========================================

	/** Get the 2D input direction from a character's CMC acceleration.
	 *  Returns zero vector if no input. Valid on dedicated server. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static FVector GetInputDirection2D(const ACharacter* Character);

	/** True if actor's ASC has IsDead or IsStunned tags. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static bool IsIncapacitated(const AActor* Actor);

	/** MW-only alignment: sets rotation warp if montage has a matching warp window.
	 *  Incapacitation check included. No-op if Direction is zero or montage has no warp window.
	 *  @return true if MW rotation warp was set. */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat")
	static bool AlignCharacterToDirection(
		AOSCharacter* Character,
		const FVector& WorldDir2D,
		UAnimMontage* Montage = nullptr,
		FName WarpTargetName = "AlignmentTarget");

	/** CMC-driven alignment: requests timer-based rotation via OSCharacterMovementComponent.
	 *  Use when MW is unavailable (no warp window on montage) or for non-montage alignment.
	 *  Incapacitation check included. No-op if Direction is zero.
	 *  @return true if CMC rotation was requested. */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat")
	static bool AlignCharacterToDirectionCMC(
		AOSCharacter* Character,
		const FVector& WorldDir2D,
		float RotationRate = 720.f,
		float MaxAngle = 180.f);

	/** Clear MW alignment state: remove warp target + clear any stale CMC target rotation. */
	UFUNCTION(BlueprintCallable, Category="OnSight|Combat")
	static void ClearAlignmentState(AOSCharacter* Character, FName WarpTargetName = "AlignmentTarget");

	/** Get aim offset yaw and pitch for blendspace input.
	 *  Computes the delta between the character's actor rotation and the player's control rotation
	 *  (mouse/gamepad look direction). Yaw is clamped to [-180, 180], Pitch to [-90, 90].
	 *  Returns zeroes for AI or characters without a PlayerController. */
	UFUNCTION(BlueprintPure, Category="OnSight|Combat")
	static void GetAimOffsetYawPitch(const ACharacter* Character, float& OutYaw, float& OutPitch);
};
