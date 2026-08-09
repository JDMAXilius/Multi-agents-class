// GA_OSmantleability.cpp
// Implementation of the mantling gameplay ability.
// Detection logic lives in OSMantleDetection.h/.cpp.
// This file handles: GAS lifecycle, montage, motion warping, state management.

#include "GAS/Abilities/GA_OSmantleability.h"
#include "AbilitySystemComponent.h"
#include "Utilities/OSMantleDetection.h"
#include "OSLogCategories.h"
#include "Data/OSGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

namespace
{
    const TCHAR* MantleTypeToString(EOSMantleType Type)
    {
        switch (Type)
        {
        case EOSMantleType::Vault: return TEXT("VAULT");
        case EOSMantleType::Climb: return TEXT("CLIMB");
        case EOSMantleType::Catch: return TEXT("CATCH");
        default: return TEXT("???");
        }
    }
}

UGA_OSmantleability::UGA_OSmantleability()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

    const FOSGameplayTags& Tags = FOSGameplayTags::Get();

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(Tags.Ability_Mantle);
    SetAssetTags(AssetTags);

    CancelAbilitiesWithTag.AddTag(Tags.Ability_Sprint);
    ActivationOwnedTags.AddTag(Tags.IsMantling);
    ActivationBlockedTags.AddTag(Tags.IsGrabbed);
    ActivationBlockedTags.AddTag(Tags.IsDead);
    ActivationBlockedTags.AddTag(Tags.IsStunned);
    ActivationBlockedTags.AddTag(Tags.IsDodging);
    ActivationBlockedTags.AddTag(Tags.IsAttacking);
    ActivationBlockedTags.AddTag(Tags.IsBlocking);
    // CC and hit-react gates — prevent mantle activation while the character is in a reaction
    // state. Without these, a mantle input during a hit reaction / knockdown / recoil / grab
    // could interrupt the reaction montage and produce a half-played animation stuck state.
    ActivationBlockedTags.AddTag(Tags.IsHitReacting);
    ActivationBlockedTags.AddTag(Tags.IsRecoiling);
    ActivationBlockedTags.AddTag(Tags.IsKnockedDown);
    ActivationBlockedTags.AddTag(Tags.IsBeingGrabbed);
    // Belt-and-suspenders: InstancedPerActor already prevents re-activation while running,
    // but this tag-based block also guards against edge cases where another ability or GE
    // might grant IsMantling externally.
    ActivationBlockedTags.AddTag(Tags.IsMantling);
}


// ========================================
// DETECTION CONFIG BUILDER
// ========================================

FOSMantleDetectionConfig UGA_OSmantleability::BuildDetectionConfig() const
{
    FOSMantleDetectionConfig Cfg;
    Cfg.MaxMantleHeight = MaxMantleHeight;
    Cfg.DetectionSphereRadius = DetectionSphereRadius;
    Cfg.MaxDetectionAngle = MaxDetectionAngle;
    Cfg.ProfileSliceCount = ProfileSliceCount;
    Cfg.InsetDetectionThreshold = InsetDetectionThreshold;
    Cfg.MaxCandidates = MaxCandidates;
    Cfg.SweepTraceRadius = SweepTraceRadius;
    Cfg.MantleTraceChannel = MantleTraceChannel;
    Cfg.MaxSurfaceAngle = MaxSurfaceAngle;
    Cfg.OverheadClearanceMargin = OverheadClearanceMargin;
    Cfg.bRequireFalling = bRequireFalling;
    Cfg.MinVerticalVelocity = MinVerticalVelocity;
    Cfg.JumpClearanceFactor = JumpClearanceFactor;
    Cfg.MinApproachSpeed = MinApproachSpeed;
    Cfg.MinVaultInputAlignment = MinVaultInputAlignment;
    Cfg.ScoreWeightDistance = ScoreWeightDistance;
    Cfg.ScoreWeightAngle = ScoreWeightAngle;
    Cfg.ScoreWeightWarpDelta = ScoreWeightWarpDelta;
    Cfg.ScoreWeightHeight = ScoreWeightHeight;
    Cfg.VelocityTraceBlend = VelocityTraceBlend;
    Cfg.VelocityBlendMinSpeed = VelocityBlendMinSpeed;
    Cfg.LedgeSampleCount = LedgeSampleCount;
    Cfg.LedgeSamplePassRatio = LedgeSamplePassRatio;
    Cfg.GravityLookaheadTime = GravityLookaheadTime;
    Cfg.MaxEdgeWarpDelta = MaxEdgeWarpDelta;
    Cfg.MaxLandingWarpDelta = MaxLandingWarpDelta;
    return Cfg;
}

// ========================================
// MONTAGE SELECTION
// ========================================

UAnimMontage* UGA_OSmantleability::SelectMontage(const FOSMantleTraceResult& TraceResult, float ActualFeetZ) const
{
    const FOSObstacleGeo& Obs = TraceResult.Obstacle;
    const FOSCharacterGeo& Char = TraceResult.Character;

    switch (Obs.Type)
    {
    case EOSMantleType::Vault:
        return VaultMontage;

    case EOSMantleType::Climb:
        return (Obs.LedgeHeight >= Char.WaistHeight()) ? ClimbHighMontage : ClimbLowMontage;

    case EOSMantleType::Catch:
    {
        // Use actual feet Z at activation time, not predicted. Gravity lookahead
        // shifts Feet.Z for detection, but at activation the character is at their
        // real position. Wrong Z → wrong Catch montage (High vs Low).
        const float FeetBelowLedge = Obs.LedgePoint.Z - ActualFeetZ;
        return (FeetBelowLedge >= Char.WaistHeight()) ? CatchHighMontage : CatchLowMontage;
    }

    default:
        return ClimbLowMontage;
    }
}

// ========================================
// CAN ACTIVATE (with position-threshold cache)
// ========================================

bool UGA_OSmantleability::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Character) return false;

    // Position-threshold cache: only re-run the full pipeline if the character
    // has moved beyond a fraction of their capsule radius since last check.
    // No timers, no ticks — cache is only evaluated here.
    if (IsCacheValid(Character))
    {
        // Input direction is the most expensive check (reads Enhanced Input stick → CMC).
        // Only evaluate after cheaper position/yaw/airborne checks pass via IsCacheValid.
        const FVector CurrentInput = UOSCombatBlueprintLibrary::GetInputDirection2D(Character);
        const float InputDelta = FVector::DistSquared(CurrentInput, CachedInputDirection);
        if (InputDelta < 0.25f) // ~30° direction change
        {
            return CachedTraceResult.bSuccess;
        }
    }

    const FOSMantleDetectionConfig Cfg = BuildDetectionConfig();
    const FOSCharacterGeo CharGeo = FOSMantleDetection::BuildCharacterGeo(Character, Cfg);
    CachedTraceResult = FOSMantleDetection::PerformTrace(GetWorld(), Character, CharGeo, Cfg);
    CachedTracePosition = Character->GetActorLocation();
    CachedTraceYaw = Character->GetActorRotation().Yaw;
    bCachedAirborne = Character->GetCharacterMovement()
        && Character->GetCharacterMovement()->IsFalling();
    CachedInputDirection = UOSCombatBlueprintLibrary::GetInputDirection2D(Character);
    bCacheValid = true;

    return CachedTraceResult.bSuccess;
}

// ========================================
// ACTIVATE
// ========================================

void UGA_OSmantleability::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    UE_LOG(LogOSMantle, Log, TEXT("Activated: %s"), *GetClass()->GetName());

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Reset completion guard for this mantle attempt.
    bMantleWorkCommitted = false;
	bSkipRollbackOnCancel = false;

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Prefer the cached trace from CanActivateAbility if still valid (character barely moved).
    // Re-running from scratch can produce a different result due to sub-frame movement between
    // CanActivateAbility (GAS gate) and ActivateAbility (execution). This causes the debug HUD
    // to show a valid green capsule but the character jumps instead because the fresh trace fails.
    // Fall back to a fresh trace if the cache is stale.
    const FOSMantleDetectionConfig Cfg = BuildDetectionConfig();
    FOSMantleTraceResult TraceResult;

    if (IsCacheValid(Character) && CachedTraceResult.bSuccess)
    {
        // NOTE: CachedTraceResult.Character contains stale Feet/Center/Velocity from
        // CanActivateAbility. Downstream code (SelectMontage, SetupMotionWarping) uses
        // Obs geometry + live actor position — do NOT read TraceResult.Character spatial
        // fields after this point.
        TraceResult = CachedTraceResult;
    }
    else
    {
        const FOSCharacterGeo CharGeo = FOSMantleDetection::BuildCharacterGeo(Character, Cfg);
        TraceResult = FOSMantleDetection::PerformTrace(GetWorld(), Character, CharGeo, Cfg);
    }

    if (!TraceResult.bSuccess)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UMotionWarpingComponent* MotionWarpComp = Character->FindComponentByClass<UMotionWarpingComponent>();
    if (!MotionWarpComp)
    {
        UE_LOG(LogOSMantle, Warning, TEXT("Character missing MotionWarpingComponent"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Save movement mode before switching to Flying (restored in EndAbility)
    UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
    if (MovementComp)
    {
        PreMantleMovementMode = MovementComp->MovementMode;
        bMovementModeChanged = true;
        MovementComp->SetMovementMode(MOVE_Flying);
        MovementComp->StopMovementImmediately();
    }

    const FOSObstacleGeo& Obs = TraceResult.Obstacle;

    // Save the character's state BEFORE any spatial modifications (pre-rotate, pre-position).
    // Restored on rejection (warp delta too large, no montage) and in EndAbility when
    // externally cancelled (death, grab, stun). NOT restored on successful completion.
    PreMantlePosition = Character->GetActorLocation();
    PreMantleRotation = Character->GetActorRotation();
    bPreMantleStateSaved = true;

    // Use the effective normal (profile consensus for inset geometry).
    // This ensures pre-rotation and approach face the actual wall surface at
    // interaction height, not the wall face at shin height.
    const FVector EffNormal = Obs.EffectiveNormal();

    // Pre-rotate to face the wall BEFORE the montage starts.
    // Eliminates rotation delta from MW so warp only handles translation.
    const FRotator FaceWallRotation = (-EffNormal).Rotation();
    Character->SetActorRotation(FRotator(0.f, FaceWallRotation.Yaw, 0.f));

    // Pre-position: move character to the wall face (capsule radius away).
    // Uses LedgeWallPoint (from profile wall-top section) if available, otherwise WallPoint.
    // For inset geometry, the profile's top-section wall face is the recessed surface —
    // approaching toward the original WallPoint would push into the protruding base.
    const FVector ApproachWallPoint = Obs.bLedgeWallSampled ? Obs.LedgeWallPoint : Obs.WallPoint;
    const FVector ApproachXY = ApproachWallPoint + EffNormal * (TraceResult.Character.CapsuleRadius + 2.f);
    const FVector ApproachPos = FVector(ApproachXY.X, ApproachXY.Y, PreMantlePosition.Z);

    // Sweep to detect geometry between current and approach position.
    // SetActorLocation with bSweep=true stops at the blocking position automatically —
    // the actor ends up at SweepHit.Location when blocked, no second call needed.
    FHitResult SweepHit;
    Character->SetActorLocation(ApproachPos, true, &SweepHit);

    const FVector FinalApproachPos = Character->GetActorLocation();

    // Validate post-approach warp delta — edge target at wall face, not behind it
    const FVector EdgePosition = FOSMantleDetection::ComputeEdgePosition(Obs);
    const float PostApproachDelta = FVector::Dist(FinalApproachPos, EdgePosition);

    if (MaxEdgeWarpDelta > 0.f && PostApproachDelta > MaxEdgeWarpDelta)
    {
        UE_LOG(LogOSMantle, Log, TEXT("Rejected at activation: post-approach edge delta %.0f > max %.0f"),
            PostApproachDelta, MaxEdgeWarpDelta);
        RestorePreMantleState(Character);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    Character->ForceNetUpdate();

    SetupMotionWarping(MotionWarpComp, TraceResult);

    const bool bUsesLanding = (Obs.Type != EOSMantleType::Vault);
    UE_LOG(LogOSMantle, Log, TEXT("Type: %s | Height: %.0f | Score: %.2f | PrePosDelta: %.0f | PostEdgeDelta: %.0f | Edge: %s  Landing: %s"),
        MantleTypeToString(Obs.Type),
        Obs.LedgeHeight, TraceResult.Score,
        FVector::Dist(PreMantlePosition, ApproachPos), PostApproachDelta,
        *EdgeWarpTargetName.ToString(),
        bUsesLanding ? *LandingWarpTargetName.ToString() : TEXT("(gravity)"));

    UAnimMontage* MontageToPlay = SelectMontage(TraceResult, Character->GetActorLocation().Z);

    if (!MontageToPlay)
    {
        UE_LOG(LogOSMantle, Warning, TEXT("No montage assigned for %s (height %.2f)"),
            MantleTypeToString(Obs.Type), Obs.LedgeHeight);
        RestorePreMantleState(Character);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, MontageToPlay, 1.0f, NAME_None,
        true,   // bStopWhenAbilityEnds — montage must stop when ability ends to prevent root motion desync
        1.0f,   // AnimRootMotionTranslationScale
        0.0f    // StartTimeSeconds
    );

    MontageTask->OnCompleted.AddDynamic(this, &UGA_OSmantleability::OnMontageCompleted);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_OSmantleability::OnMontageCancelled);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_OSmantleability::OnMontageCancelled);
    // NOTE: OnBlendOut intentionally NOT bound. With bStopWhenAbilityEnds=true, ending the
    // ability during blend-out kills the montage mid-warp — the character hasn't reached
    // the final MW target yet. The stopped montage then fires OnInterrupted, which calls
    // EndAbility again with bWasCancelled=true, triggering position rollback.
    // GA_OSGrab binds OnBlendOut because it uses bStopWhenAbilityEnds=false. We don't.

    MontageTask->ReadyForActivation();
}

// ========================================
// END ABILITY
// ========================================

void UGA_OSmantleability::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

    // Character-dependent cleanup (rollback, movement mode restore, warp target removal).
    // Runs only when the avatar is still valid. Flag resets below run unconditionally so
    // the ability state is consistent for the next activation even if the avatar is gone.
    if (Character)
    {
        // Death-cancel: skip the pre-mantle rollback so the corpse stays where the mantle
        // was interrupted instead of teleporting backward to the pre-mantle position.
        // Audit CRIT-4 — bSkipRollbackOnCancel had no setter before this gate.
        if (bWasCancelled)
        {
            if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
            {
                if (ASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsDead))
                {
                    bSkipRollbackOnCancel = true;
                }
            }
        }

        // --- External cancellation rollback ---
        // When the ability is cancelled externally (death, grab, stun), the character may
        // be mid-teleport at the approach position with pre-rotated facing. Restore them
        // to where they were before the mantle started. On successful completion (montage
        // finished naturally), the character is at the montage's final position — don't touch it.
        if (bWasCancelled)
        {
            if (bSkipRollbackOnCancel)
            {
                // End mantle without snapping back. Still restore movement mode.
                if (bMovementModeChanged)
                {
                    if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
                    {
                        MovementComp->SetMovementMode(PreMantleMovementMode);
                    }
                }
            }
            else
            {
                RestorePreMantleState(Character);
            }
        }
        else
        {
            // On success: don't restore position (montage drove to final pos),
            // but DO restore movement mode to Walking (character is on the obstacle).
            if (bMovementModeChanged)
            {
                if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
                {
                    MovementComp->SetMovementMode(MOVE_Walking);
                    // Ground-snap: settle the character to the floor immediately.
                    // Without this, the character can float 2-5cm above the surface
                    // until the next movement tick applies gravity.
                    MovementComp->FindFloor(Character->GetActorLocation(), MovementComp->CurrentFloor, false);
                    MovementComp->AdjustFloorHeight();
                }
            }
        }

        // Clean up warp targets to prevent stale warps from affecting subsequent montages.
        // RemoveWarpTarget is the engine API (UMotionWarpingComponent). The project BPFL
        // ClearWarpTarget takes AOSCharacter* — we use ACharacter* here so call the engine
        // API directly. The null-check on MWC covers the safety that the BPFL wrapper provides.
        if (UMotionWarpingComponent* MWC = Character->FindComponentByClass<UMotionWarpingComponent>())
        {
            MWC->RemoveWarpTarget(EdgeWarpTargetName);
            MWC->RemoveWarpTarget(LandingWarpTargetName);
        }
    }

    // ALWAYS reset flags, regardless of whether the Character is still valid. Previously
    // these resets were nested inside the `if (Character)` block — if the avatar was
    // destroyed before EndAbility fired, the state flags would leak into the next activation.
    bSkipRollbackOnCancel = false;
    bPreMantleStateSaved = false;
    bMovementModeChanged = false;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// Defensive cleanup path for when the ability spec is removed from the ASC without EndAbility
// firing first (rare — e.g. ClearAbility called explicitly or character torn down mid-ability).
// Mirrors the cleanup in EndAbility but unconditionally — don't assume the avatar or movement
// mode are in a specific state. Also used as a safety net for CRIT-9 (nested-cleanup gap).
void UGA_OSmantleability::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    if (ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
    {
        // Restore movement mode if we modified it. No rollback of position here — the
        // character may already have moved on from the mantle, and OnRemoveAbility is a
        // rare path for spec removal, not normal cancellation.
        if (bMovementModeChanged)
        {
            if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
            {
                MovementComp->SetMovementMode(PreMantleMovementMode);
            }
        }

        // Warp target cleanup (idempotent — RemoveWarpTarget is a no-op when the target isn't set).
        if (UMotionWarpingComponent* MWC = Character->FindComponentByClass<UMotionWarpingComponent>())
        {
            MWC->RemoveWarpTarget(EdgeWarpTargetName);
            MWC->RemoveWarpTarget(LandingWarpTargetName);
        }
    }

    // Reset flags unconditionally.
    bSkipRollbackOnCancel = false;
    bPreMantleStateSaved = false;
    bMovementModeChanged = false;

    Super::OnRemoveAbility(ActorInfo, Spec);
}


// ========================================
// DEBUG TRACE (called by OSHUD)
// ========================================

#if !UE_BUILD_SHIPPING
FOSMantleDebugData UGA_OSmantleability::DebugRunMantleTrace() const
{
    FOSMantleDebugData DebugData;
    if (!CurrentActorInfo) return DebugData;
    if (!CurrentActorInfo->AvatarActor.IsValid()) return DebugData;

    // Save and restore ALL cache state — debug trace must not pollute the real cache
    const FOSMantleTraceResult SavedResult = CachedTraceResult;
    const FVector SavedPos = CachedTracePosition;
    const float SavedYaw = CachedTraceYaw;
    const bool bSavedAirborne = bCachedAirborne;
    const FVector SavedInput = CachedInputDirection;
    const bool bSavedValid = bCacheValid;

    const ACharacter* Character = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get());
    if (Character)
    {
        const FOSMantleDetectionConfig Cfg = BuildDetectionConfig();
        const FOSCharacterGeo CharGeo = FOSMantleDetection::BuildCharacterGeo(Character, Cfg);
        FOSMantleDetection::PerformTrace(GetWorld(), Character, CharGeo, Cfg, &DebugData);
    }

    CachedTraceResult = SavedResult;
    CachedTracePosition = SavedPos;
    CachedTraceYaw = SavedYaw;
    bCachedAirborne = bSavedAirborne;
    CachedInputDirection = SavedInput;
    bCacheValid = bSavedValid;

    return DebugData;
}
#endif

// ========================================
// MOTION WARPING
// ========================================

void UGA_OSmantleability::SetupMotionWarping(UMotionWarpingComponent* MotionWarpComp, const FOSMantleTraceResult& TraceResult)
{
    if (!MotionWarpComp) return;

    const FOSObstacleGeo& Obs = TraceResult.Obstacle;
    const FVector EffNormal = Obs.EffectiveNormal();
    const FRotator FaceWallRotation = (-EffNormal).Rotation();

    // Edge target (wall face at ledge height) -- all types.
    // Uses WallPoint XY (the capsule sweep hit on the wall face), not LedgePoint XY
    // (which is DownTraceOffset behind the face). Placing the edge target behind the
    // wall face forces MW to push the character INTO the wall during the climb — collision
    // blocks the forward movement, causing the character to float straight up in place.
    // With the target at the wall face, the edge warp goes UP along the face (no collision),
    // then the landing warp pulls OVER the top (character is above the wall, no collision).
    const FVector EdgePosition = FOSMantleDetection::ComputeEdgePosition(Obs);
    MotionWarpComp->AddOrUpdateWarpTargetFromTransform(EdgeWarpTargetName, FTransform(FaceWallRotation, EdgePosition));

    UE_LOG(LogOSMantle, Verbose, TEXT("MW Edge: %s @ (%.0f, %.0f, %.0f)"),
        *EdgeWarpTargetName.ToString(), EdgePosition.X, EdgePosition.Y, EdgePosition.Z);

    if (Obs.Type != EOSMantleType::Vault)
    {
        // Landing target (past the edge) -- Climb and Catch land on top.
        // Uses EffectiveNormal for pull-over direction (matches capsule clearance).
        const FVector LandingPosition = Obs.LedgePoint + (-EffNormal * TraceResult.ValidatedPullOver);
        MotionWarpComp->AddOrUpdateWarpTargetFromTransform(LandingWarpTargetName, FTransform(FaceWallRotation, LandingPosition));
    }
    else if (Obs.bFarGroundFound && Obs.bFarGroundWalkable)
    {
        // Vault with known far-side ground: set landing target at the ground position.
        // This gives MW a smooth endpoint instead of relying on gravity alone.
        // The character vaults over the obstacle and MW smoothly places them on the far ground.
        const FVector VaultLandingPos = Obs.FarGroundPoint + FVector(0, 0, 2.f);
        MotionWarpComp->AddOrUpdateWarpTargetFromTransform(LandingWarpTargetName, FTransform(FaceWallRotation, VaultLandingPos));
    }
    else
    {
        // Vault without far ground (cliff edge): clear stale landing target.
        // Gravity handles the descent after the vault animation.
        MotionWarpComp->RemoveWarpTarget(LandingWarpTargetName);
    }
}

// ========================================
// HELPERS
// ========================================

void UGA_OSmantleability::RestorePreMantleState(ACharacter* Character)
{
    if (!Character) return;

    if (bPreMantleStateSaved)
    {
        Character->SetActorLocation(PreMantlePosition);
        Character->SetActorRotation(PreMantleRotation);
        bPreMantleStateSaved = false;
    }

    if (bMovementModeChanged)
    {
        if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
        {
            MovementComp->SetMovementMode(PreMantleMovementMode);
        }
        bMovementModeChanged = false;
    }
}

bool UGA_OSmantleability::IsCacheValid(const ACharacter* Character) const
{
    if (!bCacheValid || !Character) return false;

    const float CapsuleRadius = Character->GetCapsuleComponent()
        ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
    const float ThresholdSq = FMath::Square(CapsuleRadius * CacheInvalidationRadiusFraction);

    if (FVector::DistSquared(Character->GetActorLocation(), CachedTracePosition) > ThresholdSq)
        return false;

    const float YawDelta = FMath::Abs(FRotator::NormalizeAxis(
        Character->GetActorRotation().Yaw - CachedTraceYaw));
    if (YawDelta >= 15.f)
        return false;

    const bool bCurrentAirborne = Character->GetCharacterMovement()
        && Character->GetCharacterMovement()->IsFalling();
    if (bCurrentAirborne != bCachedAirborne)
        return false;

    return true;
}

void UGA_OSmantleability::OnMontageCompleted()
{
    // Mark work as committed before ending so any "interrupted/cancelled" callbacks arriving
    // during blend-out don't trigger a rollback (#72).
    bMantleWorkCommitted = true;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_OSmantleability::OnMontageCancelled()
{
    // If montage already completed, treat late cancellation callbacks as no-rollback.
    // Otherwise, perform a true cancellation rollback.
    const bool bTreatAsCancelled = !bMantleWorkCommitted;
    bMantleWorkCommitted = false;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bTreatAsCancelled);
}
