// OSMantleDetection.cpp
// Mantle detection engine implementation.
// Extracted from GA_OSmantleability -- performs all spatial queries
// for finding and evaluating mantle candidates.

#include "Utilities/OSMantleDetection.h"
#include "OSLogCategories.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

FVector FOSMantleDetection::ComputeEdgePosition(const FOSObstacleGeo& Obs)
{
    const FVector XY = Obs.bLedgeWallSampled ? Obs.LedgeWallPoint : Obs.WallPoint;
    // Place edge target at the ledge surface Z (not above it).
    // The montage root motion handles the pull-up arc from this position.
    return FVector(XY.X, XY.Y, Obs.LedgePoint.Z);
}

namespace MantleDetectionInternal
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
// ========================================
// CHARACTER GEOMETRY PROFILE
// ========================================

FOSCharacterGeo FOSMantleDetection::BuildCharacterGeo(
    const ACharacter* Character, const FOSMantleDetectionConfig& Config)
{
    FOSCharacterGeo Geo;

    if (!Character) return Geo;

    if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
    {
        Geo.CapsuleRadius = Capsule->GetScaledCapsuleRadius();
        Geo.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    }
    else
    {
        Geo.CapsuleRadius = 34.f;
        Geo.CapsuleHalfHeight = 88.f;
    }
    Geo.FullHeight = Geo.CapsuleHalfHeight * 2.f;

    Geo.Feet = Character->GetActorLocation();
    Geo.Center = Geo.Feet + FVector(0, 0, Geo.CapsuleHalfHeight);

    Geo.Velocity = Character->GetVelocity();
    Geo.VerticalVelocity = Geo.Velocity.Z;
    Geo.Forward = Character->GetActorForwardVector();

    if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
    {
        Geo.bAirborne = CMC->IsFalling();
        Geo.WalkableFloorZ = CMC->GetWalkableFloorZ();

        // Derive max jump apex from physics (used for jump/mantle arbitration)
        const float GravityMag = FMath::Abs(CMC->GetGravityZ());
        Geo.MaxJumpHeight = (GravityMag > 0.f)
            ? (FMath::Square(CMC->JumpZVelocity) / (2.f * GravityMag))
            : 0.f;

        // Pure player input direction for intent checks (excludes knockback/root motion).
        // Reuses existing BPFL helper that reads Enhanced Input stick → CMC → Acceleration.
        Geo.InputDirection = UOSCombatBlueprintLibrary::GetInputDirection2D(Character);

        if (Geo.bAirborne && Config.GravityLookaheadTime > 0.f)
        {
            const float GravityZ = CMC->GetGravityZ();
            Geo.Feet = Geo.Feet
                + Geo.Velocity * Config.GravityLookaheadTime
                + FVector(0, 0, 0.5f * GravityZ * Config.GravityLookaheadTime * Config.GravityLookaheadTime);
            Geo.Center = Geo.Feet + FVector(0, 0, Geo.CapsuleHalfHeight);

            // Project velocity at predicted time (gravity changes vertical velocity)
            Geo.VerticalVelocity = Geo.Velocity.Z + GravityZ * Config.GravityLookaheadTime;
        }
    }

    // Blend forward direction with velocity direction when moving.
    // This makes detection velocity-dependent: mantles that work while running toward a wall
    // may not work while stationary (different trace direction). Controlled by VelocityTraceBlend
    // (0 = always use actor forward, 1 = always use velocity) and VelocityBlendMinSpeed.
    const FVector Vel2D = FVector(Geo.Velocity.X, Geo.Velocity.Y, 0.f);
    const float HSpeed = Vel2D.Size();
    if (HSpeed >= Config.VelocityBlendMinSpeed && Config.VelocityTraceBlend > 0.f)
    {
        const FVector VelDir = Vel2D / HSpeed;
        Geo.Forward = FMath::Lerp(Character->GetActorForwardVector(), VelDir, Config.VelocityTraceBlend).GetSafeNormal();
    }

    return Geo;
}
// ========================================
// TOP-LEVEL TRACE (overlap sphere + scoring)
// ========================================

FOSMantleTraceResult FOSMantleDetection::PerformTrace(
    UWorld* World,
    const ACharacter* Character,
    const FOSCharacterGeo& CharGeo,
    const FOSMantleDetectionConfig& Config
#if !UE_BUILD_SHIPPING
    , FOSMantleDebugData* OutDebug
#endif
)
{
    FOSMantleTraceResult BestResult;

    if (!World) return BestResult;

    if (!Character) return BestResult;

    UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
    if (!MovementComp) return BestResult;

    // CharGeo is now a parameter (built by caller)

    // Use ability's Config.MaxSurfaceAngle if set, otherwise fall back to CMC
    const float WalkableFloorZ = (Config.MaxSurfaceAngle > 0.f)
        ? FMath::Cos(FMath::DegreesToRadians(Config.MaxSurfaceAngle))
        : CharGeo.WalkableFloorZ;

    const FVector RawCharacterLocation = Character->GetActorLocation();
    const bool bUsingPrediction = !FVector::PointsAreNear(CharGeo.Feet, RawCharacterLocation, 0.1f);

    // Detection radius: auto-compute if not set (default: 2x capsule radius covers typical arm-length reach)
    const float ActualDetectionRadius = (Config.DetectionSphereRadius > 0.f)
        ? Config.DetectionSphereRadius
        : (CharGeo.CapsuleRadius * 4.f);

    // Facing threshold: cos of max detection angle
    const float FacingThreshold = FMath::Cos(FMath::DegreesToRadians(Config.MaxDetectionAngle));

#if !UE_BUILD_SHIPPING
    if (OutDebug)
    {
        OutDebug->CharGeo = CharGeo;
        OutDebug->WalkableFloorZ = WalkableFloorZ;
        OutDebug->WalkableFloorAngle = (Config.MaxSurfaceAngle > 0.f)
            ? Config.MaxSurfaceAngle : MovementComp->GetWalkableFloorAngle();
        OutDebug->CapsuleRadius = CharGeo.CapsuleRadius;
        OutDebug->CapsuleHalfHeight = CharGeo.CapsuleHalfHeight;
        OutDebug->ActualLocation = RawCharacterLocation;
        OutDebug->PredictedLocation = CharGeo.Feet;
        OutDebug->bUsingPrediction = bUsingPrediction;
        OutDebug->TraceDirection = CharGeo.Forward;

        const FVector Vel2D = FVector(CharGeo.Velocity.X, CharGeo.Velocity.Y, 0.f);
        const float HSpeed = Vel2D.Size();
        OutDebug->VelocityDirection = HSpeed >= Config.VelocityBlendMinSpeed ? (Vel2D / HSpeed) : FVector::ZeroVector;
        OutDebug->VelocityBlendUsed = (HSpeed >= Config.VelocityBlendMinSpeed) ? Config.VelocityTraceBlend : 0.f;

        OutDebug->DerivedMinHeight = CharGeo.KneeHeight();
        OutDebug->DerivedHighThreshold = CharGeo.WaistHeight();
        OutDebug->DerivedJumpClearanceHeight = CharGeo.MaxJumpHeight * Config.JumpClearanceFactor;
        OutDebug->DerivedMaxVaultDepth = CharGeo.MaxVaultableDepth();
        OutDebug->DerivedMaxVaultDrop = CharGeo.MaxVaultDrop();
        OutDebug->DerivedIdealPullOver = CharGeo.IdealPullOver();
        OutDebug->DerivedDownTraceOffset = CharGeo.DownTraceForwardOffset();
        OutDebug->MaxMantleHeightUsed = Config.MaxMantleHeight;
        OutDebug->MinApproachSpeedUsed = Config.MinApproachSpeed;
        OutDebug->MinVaultInputAlignmentUsed = Config.MinVaultInputAlignment;
    }
#endif

    // ========================================
    // Step 0: Input direction early gate -- no backwards mantles
    // ========================================
    // When the player is actively pressing AWAY from their facing direction,
    // skip detection entirely. Prevents Climb/Catch on walls behind the character.
    // Vault already has its own per-candidate input alignment gate in IntentFilter.
    if (!CharGeo.InputDirection.IsNearlyZero(0.1f))
    {
        const FVector InputDir2D = FVector(CharGeo.InputDirection.X, CharGeo.InputDirection.Y, 0.f).GetSafeNormal();
        const FVector ForwardDir2D = FVector(CharGeo.Forward.X, CharGeo.Forward.Y, 0.f).GetSafeNormal();
        const float InputForwardDot = FVector::DotProduct(InputDir2D, ForwardDir2D);
        if (InputForwardDot < -0.2f)
        {
#if !UE_BUILD_SHIPPING
            if (OutDebug) OutDebug->RejectionReason = TEXT("Input opposing forward direction");
#endif
            return BestResult;
        }
    }

    // ========================================
    // Step 1: Overlap sphere -- find all nearby geometry
    // ========================================
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Character);

    // Overlap sphere at ACTUAL character center (not predicted) to avoid detecting
    // geometry the character can't actually reach from their current position
    const FVector OverlapCenter = RawCharacterLocation + FVector(0, 0, CharGeo.CapsuleHalfHeight);

    TArray<FOverlapResult> OverlapResults;
    World->OverlapMultiByChannel(
        OverlapResults, OverlapCenter, FQuat::Identity,
        Config.MantleTraceChannel, FCollisionShape::MakeSphere(ActualDetectionRadius),
        QueryParams);

    // Collect unique components that actually BLOCK the trace channel.
    // OverlapMulti returns both Block and Overlap responses. Components set to
    // OverlapAll (e.g. control point capture zones) are not valid mantle targets —
    // line traces pass through them, causing false rejections.
    TSet<UPrimitiveComponent*> UniqueComponents;
    for (const FOverlapResult& Overlap : OverlapResults)
    {
        if (Overlap.Component.IsValid()
            && Overlap.Component->GetCollisionResponseToChannel(Config.MantleTraceChannel) == ECR_Block)
        {
            UniqueComponents.Add(Overlap.Component.Get());
        }
    }

    // Collision channel fallback: if primary channel finds nothing AND it's not
    // already ECC_WorldStatic, retry with WorldStatic. Catches geometry configured
    // to block WorldStatic but not Visibility (collision-only volumes, custom blockers).
    if (UniqueComponents.Num() == 0 && Config.MantleTraceChannel != ECC_WorldStatic)
    {
        World->OverlapMultiByChannel(
            OverlapResults, OverlapCenter, FQuat::Identity,
            ECC_WorldStatic, FCollisionShape::MakeSphere(ActualDetectionRadius),
            QueryParams);
        for (const FOverlapResult& Overlap : OverlapResults)
        {
            if (Overlap.Component.IsValid()
                && Overlap.Component->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block)
            {
                UniqueComponents.Add(Overlap.Component.Get());
            }
        }
    }

    if (UniqueComponents.Num() == 0)
    {
#if !UE_BUILD_SHIPPING
        if (OutDebug) OutDebug->RejectionReason = TEXT("No geometry in detection sphere");
#endif
        return BestResult;
    }

    // ========================================
    // Step 2: Per-component targeted trace -- find wall faces
    // ========================================

    FCollisionQueryParams ClearanceParams = QueryParams;

    // Sort components by closest-surface-point distance (not origin distance).
    // Origin distance fails for large or offset-pivot meshes (L-shaped walls,
    // translated pivots) — the closest surface may be near but the origin far.
    struct FCompDist { UPrimitiveComponent* Comp; double DistSq; FVector ClosestPt; };
    TArray<FCompDist> SortedComponents;
    SortedComponents.Reserve(UniqueComponents.Num());
    for (UPrimitiveComponent* C : UniqueComponents)
    {
        FVector ClosestPt;
        const float Dist = C->GetClosestPointOnCollision(CharGeo.Center, ClosestPt);
        const double DSq = (Dist >= 0.f)
            ? FVector::DistSquared(CharGeo.Center, ClosestPt)
            : FVector::DistSquared(CharGeo.Center, C->GetComponentLocation());
        SortedComponents.Add({C, DSq, ClosestPt});
    }
    SortedComponents.Sort([](const FCompDist& A, const FCompDist& B) { return A.DistSq < B.DistSq; });

    TArray<FOSMantleTraceResult> ValidCandidates;
    int32 WallHitCount = 0;
    int32 EvaluatedCount = 0;

    for (const FCompDist& Entry : SortedComponents)
    {
        if (ValidCandidates.Num() >= Config.MaxCandidates) break;

        UPrimitiveComponent* Comp = Entry.Comp;

        // Reuse closest point from sorting phase (already computed, avoids redundant query).
        // Failure case (GetClosestPointOnCollision returned -1) was handled during sorting
        // by falling back to component origin distance.
        const FVector ClosestPoint = Entry.ClosestPt;

        // Direction from character to the component surface.
        // When very close (pressed against wall), the closest point is essentially
        // at or inside the capsule — fall back to character forward as trace direction.
        const FVector ToComp = (ClosestPoint - CharGeo.Center);
        const float Dist2D = FVector(ToComp.X, ToComp.Y, 0.f).Size();

        FVector DirToComp;
        if (Dist2D < CharGeo.CapsuleRadius)
        {
            // Character is overlapping or pressed against this component — use forward vector
            DirToComp = FVector(CharGeo.Forward.X, CharGeo.Forward.Y, 0.f).GetSafeNormal();
        }
        else
        {
            DirToComp = FVector(ToComp.X, ToComp.Y, 0.f).GetSafeNormal();
        }

        if (DirToComp.IsNearlyZero()) continue;

        // Facing check: is this component within the detection angle?
        const float FacingDot = FVector::DotProduct(CharGeo.Forward, DirToComp);
        if (FacingDot < FacingThreshold) continue;

        // Targeted wall trace toward this component.
        FHitResult BestWallHit;
        bool bFoundWall = false;

        // When pressed against geometry (capsule overlapping), pull the trace start
        // behind the character to ensure it begins OUTSIDE all collision.
        // One CapsuleRadius back-off (~34cm) is enough to clear the character's body.
        // The old 2x CapsuleRadius (~68cm) overshot thin geometry (fence posts, rails),
        // starting traces on the far side and missing them entirely.
        const bool bPressedAgainst = (Dist2D < CharGeo.CapsuleRadius);
        const float TraceBackOffset = bPressedAgainst ? CharGeo.CapsuleRadius : 0.f;

        // --- Primary: Capsule sweep covering full vertical search range ---
        // Instead of sampling discrete heights (which can miss between points),
        // sweep a tall capsule forward. This catches geometry at ANY height within
        // the search range in a single trace — fence rails, low walls, thin bars,
        // everything between shin and capsule center is covered with no gaps.
        //
        // The capsule is centered at the midpoint between shin and capsule center.
        // Its half-height covers the full range. Its radius is the configured sweep radius.
        const float SearchBottom = CharGeo.bAirborne ? 0.f : (CharGeo.KneeHeight() * 0.5f);
        const float SearchTop = CharGeo.CapsuleHalfHeight;
        const float SearchMid = (SearchBottom + SearchTop) * 0.5f;
        const float SearchHalfExtent = (SearchTop - SearchBottom) * 0.5f;

        // Capsule half-height must be >= radius for FCollisionShape::MakeCapsule.
        // If the search range is too small, fall back to a sphere at the midpoint.
        const float CapsuleSweepRadius = FMath::Max(Config.SweepTraceRadius, 1.f);
        const bool bUseCapsuleSweep = (SearchHalfExtent >= CapsuleSweepRadius);

        {
            const FVector TraceStart = CharGeo.Feet + FVector(0, 0, SearchMid) - DirToComp * TraceBackOffset;
            const float TraceLength = Dist2D + CharGeo.CapsuleRadius * 2.f + 20.f + TraceBackOffset;
            const FVector TraceEnd = TraceStart + DirToComp * TraceLength;

            FHitResult Hit;
            bool bHit = false;

            // Capsule sweep: tall shape catches geometry at any height in range.
            // Sphere fallback: when search range is too narrow for a capsule.
            const FCollisionShape SweepShape = bUseCapsuleSweep
                ? FCollisionShape::MakeCapsule(CapsuleSweepRadius, SearchHalfExtent)
                : FCollisionShape::MakeSphere(CapsuleSweepRadius);

            bHit = World->SweepSingleByChannel(
                Hit, TraceStart, TraceEnd, FQuat::Identity,
                Config.MantleTraceChannel, SweepShape, QueryParams);

            // Sweeps return ImpactNormal=(0,0,0) when starting inside geometry
            // (penetration at start). Retry with a line trace at the same height:
            // line traces handle penetrating starts and produce valid normals.
            if (bHit && Hit.ImpactNormal.IsNearlyZero())
            {
                FHitResult LineHit;
                const bool bLineHit = World->LineTraceSingleByChannel(
                    LineHit, TraceStart, TraceEnd, Config.MantleTraceChannel, QueryParams);
                if (bLineHit && !LineHit.ImpactNormal.IsNearlyZero())
                {
                    Hit = LineHit;
                }
                else
                {
                    bHit = false;
                }
            }

            // Wall filter: accept surfaces that are vertical-ish (not floors, not ceilings).
            // Z < 0.5 rejects walkable floors. Z > -0.5 rejects ceilings/undersides.
            // Also reject near-zero normals: sweeps can return very small but non-zero normals
            // (e.g., 0.001) that pass IsNearlyZero but aren't meaningful. Size2D check catches these.
            constexpr float WallNormalMaxZ = 0.65f;   // ~50 deg from vertical (catches cylinder tangent + slopes)
            constexpr float WallNormalMinZ = -0.5f;   // rejects ceiling-like
            constexpr float MinNormalMagnitude = 0.1f; // reject degenerate normals
            const float Normal2DSize = FVector2D(Hit.ImpactNormal.X, Hit.ImpactNormal.Y).Size();
            if (bHit && Normal2DSize > MinNormalMagnitude
                && Hit.ImpactNormal.Z < WallNormalMaxZ && Hit.ImpactNormal.Z > WallNormalMinZ)
            {
                BestWallHit = Hit;
                bFoundWall = true;
            }
        }

        // --- Multi-height line trace fallback ---
        // The capsule sweep can return unusable normals when its rounded ends
        // contact the top/bottom edge of an obstacle (common with cubes and
        // supergrid meshes shorter than SearchTop). The resulting steep normal
        // (Z > 0.5) fails the wall filter. Line traces at discrete heights hit
        // the flat face and return clean, perpendicular normals.
        //
        // This resolves two issues:
        //   1. Direction-dependent detection: capsule curvature produces different
        //      contact normals on different faces of the same cube.
        //   2. Inconsistent classification: bad normals propagate through the
        //      profile and vault probe, causing the same obstacle to classify
        //      differently depending on approach angle.
        if (!bFoundWall)
        {
            const float FallbackTraceLen = Dist2D + CharGeo.CapsuleRadius * 2.f + 20.f + TraceBackOffset;
            // Try progressively from low to mid — lower heights are more likely
            // to hit the flat face (above the ground, below the top edge).
            const float FallbackHeights[] = {
                SearchBottom + 10.f,
                (SearchBottom + SearchMid) * 0.5f,
                SearchMid
            };

            for (float LH : FallbackHeights)
            {
                if (LH < 5.f) continue;

                const FVector LStart = CharGeo.Feet + FVector(0, 0, LH) - DirToComp * TraceBackOffset;
                const FVector LEnd = LStart + DirToComp * FallbackTraceLen;

                FHitResult LHit;
                if (World->LineTraceSingleByChannel(LHit, LStart, LEnd, Config.MantleTraceChannel, QueryParams))
                {
                    const float N2D = FVector2D(LHit.ImpactNormal.X, LHit.ImpactNormal.Y).Size();
                    if (N2D > 0.1f && LHit.ImpactNormal.Z < 0.65f && LHit.ImpactNormal.Z > -0.5f)
                    {
                        BestWallHit = LHit;
                        bFoundWall = true;
                        break;
                    }
                }
            }
        }

        // --- Analytical fallback for thin/cylindrical geometry ---
        // When all traced heights fail (common with fence posts, thin bars, complex convex
        // decomposition, or capsule sweeps starting inside geometry), derive a synthetic wall
        // hit from the closest point data we already have from the overlap phase.
        // Direction from closest-point-on-surface to character center IS the surface normal
        // for convex shapes (mathematical property: the normal at the closest point always
        // points toward the query point). This bypasses tracing entirely.
        //
        // No distance restriction: the overlap sphere already confirmed this component is
        // nearby. Traces can fail at ANY distance on complex meshes (not just when pressed
        // against them). The closest-point direction is always a valid approximation.
        if (!bFoundWall)
        {
            const FVector FromCompToChar = FVector(CharGeo.Center.X - ClosestPoint.X,
                                                    CharGeo.Center.Y - ClosestPoint.Y, 0.f);
            FVector SyntheticNormal = FromCompToChar.GetSafeNormal();

            // Transform-aware fallback: when laterally aligned with the component
            // (XY direction is near-zero), derive normal from the component's local
            // axes. Uses the component's actual transform so rotated geometry (gates,
            // angled fences) produces correct normals — unlike AABB extents which
            // fail for non-axis-aligned geometry.
            if (SyntheticNormal.IsNearlyZero(0.01f))
            {
                const FTransform CompTransform = Comp->GetComponentTransform();
                const FVector LocalX = CompTransform.GetUnitAxis(EAxis::X);
                const FVector LocalY = CompTransform.GetUnitAxis(EAxis::Y);
                const FVector ToChar = FVector(CharGeo.Center.X - CompTransform.GetLocation().X,
                                               CharGeo.Center.Y - CompTransform.GetLocation().Y, 0.f);

                // Pick the local axis most perpendicular to the character direction.
                // For a fence along Y, LocalX is the face normal. For a wall along X, LocalY is.
                const float DotX = FMath::Abs(FVector::DotProduct(ToChar.GetSafeNormal(), FVector(LocalX.X, LocalX.Y, 0.f).GetSafeNormal()));
                const float DotY = FMath::Abs(FVector::DotProduct(ToChar.GetSafeNormal(), FVector(LocalY.X, LocalY.Y, 0.f).GetSafeNormal()));

                FVector ChosenAxis;
                if (!ToChar.IsNearlyZero())
                {
                    // Use the local axis that best faces the character
                    ChosenAxis = (DotX >= DotY)
                        ? FVector(LocalX.X, LocalX.Y, 0.f)
                        : FVector(LocalY.X, LocalY.Y, 0.f);
                    // Ensure it points toward the character
                    if (FVector::DotProduct(ChosenAxis, ToChar) < 0.f)
                        ChosenAxis = -ChosenAxis;
                }
                else
                {
                    ChosenAxis = FVector(CharGeo.Forward.X, CharGeo.Forward.Y, 0.f);
                }
                SyntheticNormal = ChosenAxis.GetSafeNormal();
            }

            if (!SyntheticNormal.IsNearlyZero())
            {
                // Project the impact point outward from the surface by CapsuleRadius.
                // ClosestPoint is ON (or inside for thin geometry) the surface. The down
                // trace offsets from ImpactPoint into the wall — if ImpactPoint is at the
                // surface, the offset lands inside geometry and misses the ledge.
                const FVector ProjectedImpact = ClosestPoint + SyntheticNormal * CharGeo.CapsuleRadius;

                BestWallHit.ImpactPoint = ProjectedImpact;
                BestWallHit.ImpactNormal = FVector(SyntheticNormal.X, SyntheticNormal.Y, 0.f);
                BestWallHit.Location = ProjectedImpact;
                BestWallHit.bBlockingHit = true;
                bFoundWall = true;
            }
        }

        if (!bFoundWall) continue;
        ++WallHitCount;

        // ========================================
        // Step 3: Evaluate this wall hit through full pipeline
        // ========================================
        ++EvaluatedCount;
        FOSMantleTraceResult CandidateResult;
        CandidateResult.Character = CharGeo;

#if !UE_BUILD_SHIPPING
        // For debug: only populate detailed trace data for the first few candidates
        FOSMantleDebugData* CandidateDebug = (OutDebug && EvaluatedCount <= 3) ? OutDebug : nullptr;
#endif

        bool bValid = EvaluateWallHit(World, CharGeo, BestWallHit, WalkableFloorZ, ClearanceParams,
            Config, CandidateResult
#if !UE_BUILD_SHIPPING
            , CandidateDebug
#endif
        );

        if (bValid)
        {
            CandidateResult.Score = ScoreCandidate(CharGeo, CandidateResult, ActualDetectionRadius, Config);
        }

        if (bValid)
        {
            ValidCandidates.Add(CandidateResult);
        }
    }

    if (ValidCandidates.Num() == 0)
    {
        if (WallHitCount == 0)
        {
#if !UE_BUILD_SHIPPING
            if (OutDebug && OutDebug->RejectionReason.IsEmpty())
                OutDebug->RejectionReason = TEXT("No wall faces found");
#endif
        }
        return BestResult;
    }

    // ========================================
    // Step 4: Pick the best candidate by score
    // ========================================
    int32 BestIdx = 0;
    float BestScore = ValidCandidates[0].Score;
    for (int32 i = 1; i < ValidCandidates.Num(); ++i)
    {
        if (ValidCandidates[i].Score > BestScore)
        {
            BestScore = ValidCandidates[i].Score;
            BestIdx = i;
        }
    }

    BestResult = ValidCandidates[BestIdx];

#if !UE_BUILD_SHIPPING
    if (OutDebug)
    {
        OutDebug->bSuccess = true;
        OutDebug->RejectionReason.Empty();
        OutDebug->ObstGeo = BestResult.Obstacle;
        OutDebug->MantleType = BestResult.Obstacle.Type;
        OutDebug->MantleHeight = BestResult.Obstacle.LedgeHeight;
        OutDebug->SurfaceAngle = BestResult.Obstacle.SurfaceAngle;

        // Warp target debug data — uses EffectiveNormal to match ActivateAbility/SetupMotionWarping
        const FVector DebugEffNormal = BestResult.Obstacle.EffectiveNormal();
        const FRotator FaceWallRot = (-DebugEffNormal).Rotation();
        // Edge target at wall face, not behind it (matches SetupMotionWarping)
        const FVector EdgePos = ComputeEdgePosition(BestResult.Obstacle);

        OutDebug->EdgeWarpLocation = EdgePos;
        OutDebug->EdgeWarpRotation = FaceWallRot;
        OutDebug->bHasEdgeWarp = true;
        OutDebug->EdgeWarpDelta = FVector::Dist(CharGeo.Feet, EdgePos);

        const float CurrentYaw = CharGeo.Forward.Rotation().Yaw;
        const float TargetYaw = FaceWallRot.Yaw;
        OutDebug->RotationDelta = FMath::Abs(FRotator::NormalizeAxis(TargetYaw - CurrentYaw));

        if (BestResult.Obstacle.Type != EOSMantleType::Vault)
        {
            const FVector LandingPos = BestResult.Obstacle.LedgePoint
                + (-DebugEffNormal * BestResult.ValidatedPullOver);
            OutDebug->LandingWarpLocation = LandingPos;
            OutDebug->LandingWarpRotation = FaceWallRot;
            OutDebug->bHasLandingWarp = true;
            OutDebug->LandingWarpDelta = FVector::Dist(EdgePos, LandingPos);
        }
        else if (BestResult.Obstacle.bFarGroundFound && BestResult.Obstacle.bFarGroundWalkable)
        {
            const FVector VaultLandingPos = BestResult.Obstacle.FarGroundPoint + FVector(0, 0, 2.f);
            OutDebug->LandingWarpLocation = VaultLandingPos;
            OutDebug->LandingWarpRotation = FaceWallRot;
            OutDebug->bHasLandingWarp = true;
            OutDebug->LandingWarpDelta = FVector::Dist(EdgePos, VaultLandingPos);
        }
    }
#endif

    // Full classification log
    const FOSObstacleGeo& Obs = BestResult.Obstacle;
    UE_LOG(LogOSMantle, Log,
        TEXT("MantleTrace OK | Type=%s | Height=%.0f | Score=%.2f | LedgeZ=%.0f | FeetZ=%.0f | "
             "Airborne=%s | VelZ=%.0f | WallAngle=%.0f | SurfAngle=%.0f | PullOver=%.1f | "
             "Candidates=%d/%d/%d"),
        MantleDetectionInternal::MantleTypeToString(Obs.Type),
        Obs.LedgeHeight, BestResult.Score,
        Obs.LedgePoint.Z, CharGeo.Feet.Z,
        CharGeo.bAirborne ? TEXT("Y") : TEXT("N"),
        CharGeo.VerticalVelocity,
        Obs.WallAngle, Obs.SurfaceAngle, BestResult.ValidatedPullOver,
        ValidCandidates.Num(), EvaluatedCount, WallHitCount);

    return BestResult;
}

// ========================================
// VERTICAL WALL PROFILE
// ========================================
// N horizontal line traces at evenly-spaced heights build a vertical cross-section
// of the wall. Provides: wall top height (for down trace start), consensus normal
// (for approach direction), and inset detection (distance variance).
//
// Three robustness measures:
// 1. Actor filter — only counts hits on the same actor as the capsule sweep hit.
//    Prevents nearby props (trash cans, railings) from contaminating the profile.
// 2. Contiguous section — consensus normal uses only the topmost unbroken run of hits.
//    Prevents hit-miss-hit patterns (archways, windows) from averaging two surfaces.
// 3. Back-off — trace starts CapsuleRadius behind character center.
//    Handles pressed-against-wall case where trace origin would be inside geometry.

bool FOSMantleDetection::ProfileWall(
    UWorld* World,
    const FOSCharacterGeo& CharGeo,
    const FVector& TraceDirection, float TraceLength, AActor* WallActor,
    const FCollisionQueryParams& QueryParams,
    const FOSMantleDetectionConfig& Config,
    FOSObstacleGeo& OutObs
#if !UE_BUILD_SHIPPING
    , FOSMantleDebugData* OutDebug
#endif
)
{
    const int32 SliceCount = FMath::Clamp(Config.ProfileSliceCount, 4, 10);
    const float BottomHeight = CharGeo.bAirborne ? -CharGeo.CapsuleHalfHeight : (CharGeo.KneeHeight() * 0.5f);
    const float TopHeight = Config.MaxMantleHeight;
    const float HeightStep = (TopHeight - BottomHeight) / FMath::Max(SliceCount - 1, 1);

    // Back-off: start traces behind character center to handle pressed-against-wall.
    // Distance is measured from character center (not trace start), so inset detection
    // is unaffected. Effective trace length increases by the back-off amount.
    const float BackOffDist = CharGeo.CapsuleRadius;
    const FVector BackOff = -TraceDirection * BackOffDist;
    const float EffectiveTraceLen = TraceLength + BackOffDist;

    // Per-slice storage (stack-allocated for small counts)
    struct FSliceResult
    {
        float Height = 0.f;
        bool bHit = false;
        float Distance = 0.f;
        FVector HitPoint = FVector::ZeroVector;
        FVector HitNormal = FVector::ZeroVector;
    };
    TArray<FSliceResult, TInlineAllocator<10>> Slices;
    Slices.SetNum(SliceCount);

#if !UE_BUILD_SHIPPING
    if (OutDebug)
    {
        OutDebug->bProfileRan = true;
        OutDebug->ProfileTraceDirection = TraceDirection;
        OutDebug->ProfileSlices.Reset(SliceCount);
    }
#endif

    // Wall normal filter constants (same criteria as PerformTrace)
    constexpr float WallNormalMaxZ = 0.65f;   // ~50 deg from vertical (matches PerformTrace)
    constexpr float WallNormalMinZ = -0.5f;
    constexpr float MinNormalMag = 0.1f;

    // --- Fire N horizontal line traces ---
    for (int32 i = 0; i < SliceCount; ++i)
    {
        FSliceResult& S = Slices[i];
        S.Height = BottomHeight + (HeightStep * i);

        const FVector Origin = FVector(CharGeo.Center.X, CharGeo.Center.Y, CharGeo.Feet.Z + S.Height);
        const FVector Start = Origin + BackOff;
        const FVector End = Origin + TraceDirection * EffectiveTraceLen;

        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, Start, End, Config.MantleTraceChannel, QueryParams))
        {
            // Actor filter: only accept hits on the same actor that the capsule sweep found.
            // WallActor == nullptr bypasses the check (analytical fallback case).
            const bool bSameActor = (WallActor == nullptr) || (Hit.GetActor() == WallActor);

            const float Norm2D = FVector2D(Hit.ImpactNormal.X, Hit.ImpactNormal.Y).Size();
            const bool bValidWall = bSameActor
                && Norm2D > MinNormalMag
                && Hit.ImpactNormal.Z < WallNormalMaxZ
                && Hit.ImpactNormal.Z > WallNormalMinZ;

            if (bValidWall)
            {
                S.bHit = true;
                S.HitPoint = Hit.ImpactPoint;
                S.HitNormal = Hit.ImpactNormal;
                // Distance from character center (NOT trace start) — unaffected by back-off
                S.Distance = FVector::Dist2D(Origin, Hit.ImpactPoint);
            }
        }

#if !UE_BUILD_SHIPPING
        if (OutDebug)
        {
            FOSMantleDebugData::FOSProfileSliceDebug SD;
            SD.Height = S.Height;
            SD.bHit = S.bHit;
            if (S.bHit) { SD.HitPoint = S.HitPoint; SD.HitNormal = S.HitNormal; SD.Distance = S.Distance; }
            OutDebug->ProfileSlices.Add(SD);
        }
#endif
    }

    // --- Profile coverage (set before early return so 0-hit case is captured) ---
    OutObs.bProfileRan = true;
    {
        int32 HitCount = 0;
        for (const FSliceResult& S : Slices) { if (S.bHit) ++HitCount; }
        OutObs.ProfileHitRatio = static_cast<float>(HitCount) / SliceCount;
    }

    // --- Find wall top (highest hit slice) ---
    int32 WallTopIndex = INDEX_NONE;
    for (int32 i = SliceCount - 1; i >= 0; --i)
    {
        if (Slices[i].bHit) { WallTopIndex = i; break; }
    }

    if (WallTopIndex == INDEX_NONE) return false;

    // --- Reliability gate ---
    // When coverage is low (e.g. 1/6 on a leaning wall), derived data is unreliable.
    // Only override pipeline defaults (WallTopHeight, Phase 2b, inset) when we have
    // enough hits to trust. Below threshold: defaults remain → down trace uses proven
    // MaxMH+HH height, EffectiveNormal() returns WallNormal from capsule sweep.
    constexpr float ReliableProfileThreshold = 0.33f; // 2/6 slices = reliable (was 0.5)
    const bool bReliableProfile = OutObs.ProfileHitRatio >= ReliableProfileThreshold;

    // --- Consensus normal from topmost CONTIGUOUS section (gap-safe) ---
    // Always compute consensus for debug visualization, but only write to Phase 2b
    // fields when profile is reliable.
    constexpr int32 MaxConsensusSlices = 3;
    TArray<FVector, TInlineAllocator<4>> TopNormals;
    const int32 ContiguousTopIndex = WallTopIndex;  // the top of the contiguous section
    for (int32 i = WallTopIndex; i >= 0 && TopNormals.Num() < MaxConsensusSlices; --i)
    {
        if (!Slices[i].bHit) break;  // gap — stop, only use contiguous top section
        TopNormals.Add(Slices[i].HitNormal);
    }

    FVector ConsensusNormal = OutObs.WallNormal; // fallback to original capsule sweep normal
    if (TopNormals.Num() >= 3)
    {
        // Two-pass outlier rejection:
        // 1. Compute initial mean from all contiguous top normals.
        // 2. Reject normals > 30° from the mean, re-average survivors.
        // Uses vector average instead of atan2-sorted median to avoid
        // the atan2 discontinuity at ±π (normals near ±180° were mishandled).
        FVector InitialSum = FVector::ZeroVector;
        for (const FVector& N : TopNormals) { InitialSum += N; }
        const FVector Mean = InitialSum.GetSafeNormal();

        constexpr float OutlierDot = 0.866f; // cos(30 degrees)
        FVector Sum = FVector::ZeroVector;
        int32 Count = 0;
        for (const FVector& N : TopNormals)
        {
            if (FVector::DotProduct(N, Mean) >= OutlierDot) { Sum += N; ++Count; }
        }
        ConsensusNormal = Count > 0 ? Sum.GetSafeNormal() : Mean.GetSafeNormal();
    }
    else if (TopNormals.Num() > 0)
    {
        // Too few slices for outlier rejection — weight the top slice (closest to ledge
        // interaction height) 2x to prefer the surface the character actually grabs.
        FVector Sum = FVector::ZeroVector;
        for (int32 i = 0; i < TopNormals.Num(); ++i)
        {
            Sum += TopNormals[i] * (i == 0 ? 2.f : 1.f);
        }
        ConsensusNormal = Sum.GetSafeNormal();
    }
    if (ConsensusNormal.IsNearlyZero()) ConsensusNormal = OutObs.WallNormal;

    // --- Write derived data only when profile is trustworthy ---
    if (bReliableProfile)
    {
        // WallTopHeight: highest hit — the down trace must start above ALL geometry.
        OutObs.WallTopHeight = Slices[WallTopIndex].Height;

        // Phase 2b: LedgeWallPoint from contiguous section's top slice, consensus normal.
        // → EffectiveNormal() will return this instead of raw WallNormal.
        OutObs.bLedgeWallSampled = true;
        OutObs.LedgeWallPoint = Slices[ContiguousTopIndex].HitPoint;
        OutObs.LedgeWallNormal = ConsensusNormal;

        // Inset detection (across ALL hits, not just contiguous section).
        // A protruding base at shin height IS an inset, even if there's a gap above it.
        float MinDist = TNumericLimits<float>::Max();
        float MaxDist = 0.f;
        for (const FSliceResult& S : Slices)
        {
            if (S.bHit) { MinDist = FMath::Min(MinDist, S.Distance); MaxDist = FMath::Max(MaxDist, S.Distance); }
        }
        OutObs.InsetDepth = MaxDist - MinDist;
        OutObs.bInsetDetected = (OutObs.InsetDepth > Config.InsetDetectionThreshold);
    }
    else if (WallTopIndex != INDEX_NONE)
    {
        // Low coverage but at least one hit: use the top hit's normal for rotation.
        // Better to face the one surface we found than fall back to the capsule-sweep
        // normal which may be from a completely different height/angle.
        // Don't populate WallTopHeight or inset data — those need reliable coverage.
        OutObs.bLedgeWallSampled = true;
        OutObs.LedgeWallPoint = Slices[WallTopIndex].HitPoint;
        OutObs.LedgeWallNormal = Slices[WallTopIndex].HitNormal;
    }
    // else: zero hits — all fields stay at defaults
    // → down trace falls back to proven MaxMH+HH start height
    // → EffectiveNormal() returns WallNormal from capsule sweep

#if !UE_BUILD_SHIPPING
    if (OutDebug)
    {
        OutDebug->ProfileWallTopIndex = WallTopIndex;
        OutDebug->bProfileInsetDetected = OutObs.bInsetDetected;
        OutDebug->ProfileInsetDepth = OutObs.InsetDepth;
        OutDebug->ProfileConsensusNormal = ConsensusNormal;
    }
#endif

    return true;
}
// ========================================
// EVALUATE SINGLE WALL HIT - Context
// ========================================

// Pipeline scratchpad shuttling intermediate state between the EvalStep_* steps.
// Pipeline scratchpad shuttling intermediate state between the EvalStep_* steps.
struct FOSMantleDetection::FEvalCtx
{
    // --- Inputs ---
    const FOSCharacterGeo& CharGeo;
    const FHitResult& WallHit;
    float WalkableFloorZ;
    FCollisionQueryParams QueryParams;
    FCollisionQueryParams OverheadParams;
    const FOSMantleDetectionConfig& Config;
    FOSMantleTraceResult& OutResult;
#if !UE_BUILD_SHIPPING
    FOSMantleDebugData* OutDebug;
#endif

    // --- Derived constants ---
    float MinHeight;
    float DownTraceOffset;
    float VaultDepth;
    float VaultDrop;
    float IdealPullOver;
    float MinPullOverDist;

    // --- Intermediate state ---
    FVector EffNormal;
    FHitResult DownHit;
    FVector DownTraceStart;
    FVector DownTraceEnd;
    FVector WallNormal;
    FVector CapsuleCheckLocation;
    float FinalPullOver;

    FEvalCtx(const FOSCharacterGeo& InCharGeo, const FHitResult& InWallHit,
        float InWalkableFloorZ, const FCollisionQueryParams& InParams,
        const FOSMantleDetectionConfig& InConfig,
        FOSMantleTraceResult& InOutResult
#if !UE_BUILD_SHIPPING
        , FOSMantleDebugData* InDebug
#endif
    )
        : CharGeo(InCharGeo)
        , WallHit(InWallHit)
        , WalkableFloorZ(InWalkableFloorZ)
        , QueryParams(InParams)
        , OverheadParams(InParams)
        , Config(InConfig)
        , OutResult(InOutResult)
#if !UE_BUILD_SHIPPING
        , OutDebug(InDebug)
#endif
        , MinHeight(InCharGeo.KneeHeight())
        , DownTraceOffset(InCharGeo.DownTraceForwardOffset())
        , VaultDepth(InCharGeo.MaxVaultableDepth())
        , VaultDrop(InCharGeo.MaxVaultDrop())
        , IdealPullOver(InCharGeo.IdealPullOver())
        , MinPullOverDist(InCharGeo.MinPullOver())
        , EffNormal(FVector::ZeroVector)
        , DownTraceStart(FVector::ZeroVector)
        , DownTraceEnd(FVector::ZeroVector)
        , WallNormal(FVector::ZeroVector)
        , CapsuleCheckLocation(FVector::ZeroVector)
        , FinalPullOver(InCharGeo.IdealPullOver())
    {
    }
};

// ========================================
// EVALUATE SINGLE WALL HIT - Orchestrator
// ========================================

bool FOSMantleDetection::EvaluateWallHit(
    UWorld* World,
    const FOSCharacterGeo& CharGeo,
    const FHitResult& WallHit,
    float WalkableFloorZ,
    const FCollisionQueryParams& ClearanceParams,
    const FOSMantleDetectionConfig& Config,
    FOSMantleTraceResult& OutResult
#if !UE_BUILD_SHIPPING
    , FOSMantleDebugData* OutDebug
#endif
)
{
	FEvalCtx Ctx(CharGeo, WallHit, WalkableFloorZ, ClearanceParams, Config, OutResult
#if !UE_BUILD_SHIPPING
		, OutDebug
#endif
	);

	if (!EvalStep_InitAndProfile(World, Ctx)) return false;
	if (!EvalStep_FindLedge(World, Ctx)) return false;
	if (!EvalStep_CapsuleClearance(World, Ctx)) return false;
	if (!EvalStep_ApproachPath(World, Ctx)) return false;

	// Build overhead params (shared by edge + landing checks)
	if (WallHit.GetActor())
		Ctx.OverheadParams.AddIgnoredActor(WallHit.GetActor());

	if (!EvalStep_EdgeOverhead(World, Ctx)) return false;
	if (!EvalStep_LandingOverhead(World, Ctx)) return false;
	if (!EvalStep_LandingFloor(World, Ctx)) return false;
	EvalStep_Classify(World, Ctx);

	// Deferred floor rejection: Climb/Catch need walkable on-top floor.
	// Vaults skip — they land on the far side (checked in Classify's far-ground trace).
	if (Ctx.OutResult.Obstacle.Type != EOSMantleType::Vault)
	{
		const FOSObstacleGeo& FloorObs = Ctx.OutResult.Obstacle;
		if (!FloorObs.bOnTopFloorFound)
		{
			Ctx.OutResult.RejectionReason = TEXT("No floor at landing");
			return false;
		}
		if (!FloorObs.bOnTopFloorWalkable)
		{
			Ctx.OutResult.RejectionReason = TEXT("Floor not walkable");
			return false;
		}
	}

	if (!EvalStep_IntentFilter(Ctx)) return false;
	if (!EvalStep_LedgeWidth(World, Ctx)) return false;
	if (!EvalStep_DeferredFalling(Ctx)) return false;
	if (!EvalStep_WarpDelta(Ctx)) return false;

	OutResult.bSuccess = true;
	OutResult.ValidatedPullOver = Ctx.FinalPullOver;
	return true;
}

// ========================================
// EVALUATE SINGLE WALL HIT - Steps
// ========================================

// --- Step 1: Wall setup + vertical profile ---
bool FOSMantleDetection::EvalStep_InitAndProfile(UWorld* World, FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	FOSObstacleGeo& ObsGeo = Ctx.OutResult.Obstacle;
	ObsGeo.bWallFound = true;
	ObsGeo.WallPoint = Ctx.WallHit.ImpactPoint;
	ObsGeo.WallNormal = Ctx.WallHit.ImpactNormal;
	ObsGeo.WallAngle = FMath::RadiansToDegrees(FMath::Acos(
		FMath::Clamp(FVector::DotProduct(Ctx.WallHit.ImpactNormal, FVector::UpVector), -1.f, 1.f)));

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->ForwardHitPoint = Ctx.WallHit.ImpactPoint;
		Ctx.OutDebug->ForwardHitNormal = Ctx.WallHit.ImpactNormal;
		Ctx.OutDebug->ForwardHitAngle = ObsGeo.WallAngle;
		Ctx.OutDebug->bForwardHit = true;
	}
#endif

	const FVector ProfileDir = -Ctx.WallHit.ImpactNormal;
	const float HorizDist = FVector::Dist2D(Ctx.CharGeo.Center, Ctx.WallHit.ImpactPoint);
	const float ProfileLen = HorizDist + Ctx.CharGeo.CapsuleRadius * 2.f + 20.f;

	ProfileWall(World, Ctx.CharGeo, ProfileDir, ProfileLen, Ctx.WallHit.GetActor(), Ctx.QueryParams, Ctx.Config, ObsGeo
#if !UE_BUILD_SHIPPING
		, Ctx.OutDebug
#endif
	);

	return true;
}

// --- Step 2: Down trace / ledge find ---
bool FOSMantleDetection::EvalStep_FindLedge(UWorld* World, FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	FOSObstacleGeo& ObsGeo = Ctx.OutResult.Obstacle;

	Ctx.EffNormal = ObsGeo.EffectiveNormal();
	const FVector WallPastDir = -Ctx.EffNormal;

	const float ProfileBottomHeight = Ctx.CharGeo.bAirborne ? -Ctx.CharGeo.CapsuleHalfHeight : (Ctx.CharGeo.KneeHeight() * 0.5f);
	const float ProfileHeightStep = (Config.MaxMantleHeight - ProfileBottomHeight) / FMath::Max(Config.ProfileSliceCount - 1, 1);
	constexpr float DownSweepRadius = 5.f;
	const float DownTraceStartZ = (ObsGeo.WallTopHeight > 0.f)
		? (Ctx.CharGeo.Feet.Z + ObsGeo.WallTopHeight + ProfileHeightStep + DownSweepRadius)
		: (Ctx.WallHit.ImpactPoint.Z + Ctx.CharGeo.DownTraceSearchHeight(Config.MaxMantleHeight));

	const float DownTraceEndZ = Ctx.CharGeo.bAirborne
		? (Ctx.CharGeo.Feet.Z - Ctx.CharGeo.CapsuleHalfHeight)
		: Ctx.CharGeo.Feet.Z;

	// Two-pass down trace: first at standard offset (CapsuleRadius + 5cm, good for thick walls),
	// then retry at a close offset (DownSweepRadius * 2 + 1cm) if the standard offset found
	// only ground-level geometry. The standard 47cm offset overshoots thin fences — the
	// trace starts behind the fence and hits the ground, missing the fence top entirely.
	// The close offset stays near the wall face where thin geometry tops are reachable.
	const float StandardOffset = Ctx.DownTraceOffset;
	const float CloseOffset = DownSweepRadius * 2.f + 1.f; // 11cm — just past wall face
	const float DownTraceOffsets[] = { StandardOffset, CloseOffset };

	// Capture standard-offset positions for debug before the loop potentially overwrites them.
	const FVector StandardDownTraceXY = Ctx.WallHit.ImpactPoint + (WallPastDir * StandardOffset);
	Ctx.DownTraceStart = FVector(StandardDownTraceXY.X, StandardDownTraceXY.Y, DownTraceStartZ);
	Ctx.DownTraceEnd = FVector(StandardDownTraceXY.X, StandardDownTraceXY.Y, DownTraceEndZ);

	bool bDownHit = false;
	for (float CurrentOffset : DownTraceOffsets)
	{
		// Skip close retry if standard already found a valid ledge above knee height,
		// or if close offset is the same as standard (small capsule).
		if (bDownHit && (Ctx.DownHit.ImpactPoint.Z - Ctx.CharGeo.Feet.Z) >= Ctx.MinHeight)
			break;
		if (CurrentOffset == CloseOffset && FMath::Abs(StandardOffset - CloseOffset) < 1.f)
			break;

		const FVector DownTraceXY = Ctx.WallHit.ImpactPoint + (WallPastDir * CurrentOffset);
		const FVector LoopStart = FVector(DownTraceXY.X, DownTraceXY.Y, DownTraceStartZ);
		const FVector LoopEnd = FVector(DownTraceXY.X, DownTraceXY.Y, DownTraceEndZ);

		bDownHit = World->SweepSingleByChannel(
			Ctx.DownHit, LoopStart, LoopEnd, FQuat::Identity,
			Config.MantleTraceChannel, FCollisionShape::MakeSphere(DownSweepRadius), Ctx.QueryParams);
	}

	// Refine Z with a line trace to eliminate sphere sweep Z bias.
	// The sphere sweep reliably finds WHICH surface exists, but its ImpactPoint.Z is
	// biased upward on curved/sloped surfaces by up to the sweep radius.
	if (bDownHit)
	{
		const FVector RefineStart = FVector(Ctx.DownHit.ImpactPoint.X, Ctx.DownHit.ImpactPoint.Y,
			Ctx.DownHit.ImpactPoint.Z + DownSweepRadius + 1.f);
		const FVector RefineEnd = FVector(Ctx.DownHit.ImpactPoint.X, Ctx.DownHit.ImpactPoint.Y,
			Ctx.DownHit.ImpactPoint.Z - DownSweepRadius - 1.f);
		FHitResult RefineHit;
		if (World->LineTraceSingleByChannel(RefineHit, RefineStart, RefineEnd,
			Config.MantleTraceChannel, Ctx.QueryParams))
		{
			Ctx.DownHit.ImpactPoint = RefineHit.ImpactPoint;
			Ctx.DownHit.ImpactNormal = RefineHit.ImpactNormal;
		}
	}

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->DownStart = Ctx.DownTraceStart;
		Ctx.OutDebug->DownEnd = Ctx.DownTraceEnd;
		Ctx.OutDebug->DownSweepRadius = DownSweepRadius;
		Ctx.OutDebug->bDownHit = bDownHit;
		if (bDownHit)
		{
			Ctx.OutDebug->DownHitPoint = Ctx.DownHit.ImpactPoint;
			Ctx.OutDebug->DownHitNormal = Ctx.DownHit.ImpactNormal;
		}
	}
#endif

	if (!bDownHit) { Ctx.OutResult.RejectionReason = TEXT("No ledge surface"); return false; }
	if (Ctx.DownHit.ImpactNormal.Z < Ctx.WalkableFloorZ) { Ctx.OutResult.RejectionReason = TEXT("Ledge not walkable"); return false; }

	// Verify wall and ledge belong to the same actor.
	// Stacked geometry (e.g., box on box) can produce phantom candidates where the wall face
	// is on the lower object but the down trace finds the upper object's top surface.
	// The resulting edge warp target would be in mid-air between the two objects.
	if (Ctx.WallHit.GetActor() && Ctx.DownHit.GetActor()
		&& Ctx.WallHit.GetActor() != Ctx.DownHit.GetActor())
	{
		Ctx.OutResult.RejectionReason = TEXT("Wall and ledge on different actors");
		return false;
	}

	// Verify wall and ledge are horizontally coherent.
	// Compound collision (same actor, different primitives) can produce wall XY from
	// one primitive and ledge XY from another — the resulting edge target would be in mid-air.
	const float WallLedgeXYDist = FVector::Dist2D(Ctx.WallHit.ImpactPoint, Ctx.DownHit.ImpactPoint);
	if (WallLedgeXYDist > Ctx.CharGeo.CapsuleRadius * 3.f)
	{
		Ctx.OutResult.RejectionReason = FString::Printf(TEXT("Wall-ledge XY gap: %.0f cm"), WallLedgeXYDist);
		return false;
	}

	ObsGeo.bLedgeFound = true;
	ObsGeo.LedgePoint = Ctx.DownHit.ImpactPoint;
	ObsGeo.LedgeNormal = Ctx.DownHit.ImpactNormal;
	ObsGeo.LedgeHeight = Ctx.DownHit.ImpactPoint.Z - Ctx.CharGeo.Feet.Z;
	ObsGeo.SurfaceAngle = FMath::RadiansToDegrees(FMath::Acos(
		FMath::Clamp(FVector::DotProduct(Ctx.DownHit.ImpactNormal, FVector::UpVector), -1.f, 1.f)));

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->MantleHeight = ObsGeo.LedgeHeight;
		Ctx.OutDebug->SurfaceAngle = ObsGeo.SurfaceAngle;
	}
#endif

	const FVector CharToLedge2D = FVector(Ctx.DownHit.ImpactPoint.X - Ctx.CharGeo.Feet.X,
		Ctx.DownHit.ImpactPoint.Y - Ctx.CharGeo.Feet.Y, 0.f);
	const float MaxHorizontalReach = (Config.DetectionSphereRadius > 0.f)
		? Config.DetectionSphereRadius : (Ctx.CharGeo.CapsuleRadius * 4.f);
	if (CharToLedge2D.Size() > MaxHorizontalReach * 1.5f)
	{
		Ctx.OutResult.RejectionReason = FString::Printf(TEXT("Too far: %.0f cm"), CharToLedge2D.Size());
		return false;
	}

	if (Ctx.CharGeo.bAirborne)
	{
		if (ObsGeo.LedgeHeight > Config.MaxMantleHeight || ObsGeo.LedgeHeight < 0.f)
		{
			Ctx.OutResult.RejectionReason = FString::Printf(TEXT("Airborne height %.0f outside range"), ObsGeo.LedgeHeight);
			return false;
		}
	}
	else
	{
		// Use a low minimum (20cm) to allow short vault-height obstacles through.
		// The KneeHeight minimum for CLIMB type is enforced later in IntentFilter,
		// after classification determines whether this is a vault or climb.
		// Without this, fences at 30-50cm are rejected before they can be classified as vaults.
		constexpr float AbsoluteMinHeight = 20.f;
		if (ObsGeo.LedgeHeight < AbsoluteMinHeight || ObsGeo.LedgeHeight > Config.MaxMantleHeight)
		{
			Ctx.OutResult.RejectionReason = FString::Printf(TEXT("Height %.0f not in [%.0f, %.0f]"),
				ObsGeo.LedgeHeight, AbsoluteMinHeight, Config.MaxMantleHeight);
			return false;
		}
	}

	return true;
}

// --- Step 3: Capsule clearance (iterative pull-over) ---
bool FOSMantleDetection::EvalStep_CapsuleClearance(UWorld* World, FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	Ctx.WallNormal = Ctx.OutResult.Obstacle.EffectiveNormal();
	const float PullOverStep = Ctx.CharGeo.CapsuleRadius * 0.25f;
	const int32 MaxAttempts = FMath::CeilToInt((Ctx.IdealPullOver - Ctx.MinPullOverDist) / FMath::Max(PullOverStep, 1.f)) + 1;

	bool bFoundClearPosition = false;
	int32 WalkableFiltered = 0;
	TArray<FHitResult> ActualObstacles;
	TArray<FHitResult> FirstAttemptObstacles;
	int32 FirstAttemptWalkableFiltered = 0;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const float CurrentPullOver = FMath::Max(Ctx.IdealPullOver - (PullOverStep * Attempt), Ctx.MinPullOverDist);
		Ctx.CapsuleCheckLocation = Ctx.DownHit.ImpactPoint
			+ FVector(0, 0, Ctx.CharGeo.CapsuleHalfHeight)
			+ (-Ctx.WallNormal * CurrentPullOver);

		TArray<FHitResult> OverlapResults;
		World->SweepMultiByChannel(
			OverlapResults, Ctx.CapsuleCheckLocation, Ctx.CapsuleCheckLocation, FQuat::Identity,
			Config.MantleTraceChannel, FCollisionShape::MakeCapsule(Ctx.CharGeo.CapsuleRadius, Ctx.CharGeo.CapsuleHalfHeight),
			Ctx.QueryParams);

		WalkableFiltered = 0;
		ActualObstacles.Reset();
		for (const FHitResult& Overlap : OverlapResults)
		{
			if (Overlap.ImpactNormal.IsNearlyZero() || Overlap.ImpactNormal.Z < Ctx.WalkableFloorZ)
				ActualObstacles.Add(Overlap);
			else
				++WalkableFiltered;
		}

		if (Attempt == 0)
		{
			FirstAttemptObstacles = ActualObstacles;
			FirstAttemptWalkableFiltered = WalkableFiltered;
		}

		if (ActualObstacles.Num() == 0)
		{
			Ctx.FinalPullOver = CurrentPullOver;
			bFoundClearPosition = true;
			break;
		}

		if (CurrentPullOver <= Ctx.MinPullOverDist) break;
	}

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->CapsuleCheckCenter = Ctx.CapsuleCheckLocation;
		Ctx.OutDebug->bCapsuleBlocked = !bFoundClearPosition;
		Ctx.OutDebug->ClearanceOverlapTotal = FirstAttemptObstacles.Num() + FirstAttemptWalkableFiltered;
		Ctx.OutDebug->ClearanceWalkableFiltered = FirstAttemptWalkableFiltered;

		if (!bFoundClearPosition)
		{
			TArray<FString> Details;
			for (const FHitResult& Obs : FirstAttemptObstacles)
			{
				const FString Name = Obs.GetActor() ? Obs.GetActor()->GetName() : TEXT("?");
				Details.Add(FString::Printf(TEXT("%s (N=%.2f,%.2f,%.2f)"),
					*Name, Obs.ImpactNormal.X, Obs.ImpactNormal.Y, Obs.ImpactNormal.Z));
			}
			Ctx.OutDebug->BlockerDetails = FString::Join(Details, TEXT(", "));
		}
	}
#endif

	if (!bFoundClearPosition)
	{
		Ctx.OutResult.RejectionReason = TEXT("Capsule blocked at all positions");
		return false;
	}
	return true;
}

// --- Step 4: Approach path clearance ---
bool FOSMantleDetection::EvalStep_ApproachPath(UWorld* World, FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	const FOSObstacleGeo& ObsGeo = Ctx.OutResult.Obstacle;
	const FVector ApproachWallPt = ObsGeo.bLedgeWallSampled ? ObsGeo.LedgeWallPoint : ObsGeo.WallPoint;
	const FVector ApproachXY = ApproachWallPt + Ctx.WallNormal * (Ctx.CharGeo.CapsuleRadius + 2.f);
	const FVector EndOffset = Ctx.WallNormal * (Ctx.CharGeo.CapsuleRadius * 0.5f);
	const FVector PathEndXY = FVector(ApproachWallPt.X + EndOffset.X, ApproachWallPt.Y + EndOffset.Y, 0.f);

	// --- Check 1: Vertical overhead at approach position ---
	// Verify the character has enough headroom at the wall face to start the climb.
	// Overhangs (roof eaves, awnings) extend out past the wall face at a height below
	// the ledge. The diagonal approach trace (Check 2) hugs the wall and passes UNDER
	// overhangs. This vertical check catches them by tracing straight up from the
	// character's head at the approach position to the ledge height.
	{
		const FVector OverheadBase = FVector(ApproachXY.X, ApproachXY.Y,
			Ctx.CharGeo.Feet.Z + Ctx.CharGeo.FullHeight);
		const FVector OverheadEnd = FVector(ApproachXY.X, ApproachXY.Y,
			ObsGeo.LedgePoint.Z + EdgeHeightOffset);

		if (OverheadEnd.Z > OverheadBase.Z)
		{
			FHitResult OverhangHit;
			const bool bOverhangBlocked = World->LineTraceSingleByChannel(
				OverhangHit, OverheadBase, OverheadEnd, Config.MantleTraceChannel, Ctx.QueryParams);

			if (bOverhangBlocked)
			{
				const float OverhangHeight = OverhangHit.ImpactPoint.Z - Ctx.CharGeo.Feet.Z;
#if !UE_BUILD_SHIPPING
				if (Ctx.OutDebug)
				{
					Ctx.OutDebug->bApproachOverheadCheckRan = true;
					Ctx.OutDebug->ApproachOverheadStart = OverheadBase;
					Ctx.OutDebug->ApproachOverheadEnd = OverheadEnd;
					Ctx.OutDebug->bApproachOverheadBlocked = true;
					Ctx.OutDebug->ApproachOverheadHitPoint = OverhangHit.ImpactPoint;
				}
#endif
				Ctx.OutResult.RejectionReason = FString::Printf(
					TEXT("Overhang at approach: %.0f cm (need %.0f to reach ledge)"),
					OverhangHeight, ObsGeo.LedgeHeight);
				return false;
			}
		}
	}

	// --- Check 2: Diagonal approach path clearance ---
	// Traces from approach position at feet height to the ledge edge. Catches
	// obstructions along the climb path (ceilings, cross-beams, geometry
	// between the character and the ledge).
	const FVector PathStart = FVector(ApproachXY.X, ApproachXY.Y, Ctx.CharGeo.Feet.Z + 10.f);
	const FVector PathEnd = FVector(PathEndXY.X, PathEndXY.Y, ObsGeo.LedgePoint.Z + EdgeHeightOffset);

	if (PathEnd.Z > PathStart.Z)
	{
		FHitResult ApproachOHHit;
		const bool bApproachBlocked = World->LineTraceSingleByChannel(
			ApproachOHHit, PathStart, PathEnd, Config.MantleTraceChannel, Ctx.QueryParams);

#if !UE_BUILD_SHIPPING
		if (Ctx.OutDebug)
		{
			Ctx.OutDebug->bApproachOverheadCheckRan = true;
			Ctx.OutDebug->ApproachOverheadStart = PathStart;
			Ctx.OutDebug->ApproachOverheadEnd = PathEnd;
			Ctx.OutDebug->bApproachOverheadBlocked = bApproachBlocked;
			if (bApproachBlocked)
				Ctx.OutDebug->ApproachOverheadHitPoint = ApproachOHHit.ImpactPoint;
		}
#endif

		if (bApproachBlocked)
		{
			const float OverhangZ = ApproachOHHit.ImpactPoint.Z - Ctx.CharGeo.Feet.Z;
			Ctx.OutResult.RejectionReason = FString::Printf(
				TEXT("Overhead blocked on approach: overhang at %.0f cm (ledge at %.0f)"),
				OverhangZ, ObsGeo.LedgeHeight);
			return false;
		}
	}
	return true;
}

// --- Step 5: Edge overhead clearance ---
bool FOSMantleDetection::EvalStep_EdgeOverhead(UWorld* World, FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	const FVector EdgeTop = Ctx.DownHit.ImpactPoint + FVector(0, 0, Ctx.CharGeo.FullHeight);
	const FVector EdgeOverheadEnd = EdgeTop + FVector(0, 0, Config.OverheadClearanceMargin);

	FHitResult EdgeOverheadHit;
	const bool bEdgeBlocked = World->LineTraceSingleByChannel(
		EdgeOverheadHit, EdgeTop, EdgeOverheadEnd, Config.MantleTraceChannel, Ctx.OverheadParams);

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->bEdgeOverheadCheckRan = true;
		Ctx.OutDebug->EdgeOverheadStart = EdgeTop;
		Ctx.OutDebug->EdgeOverheadEnd = EdgeOverheadEnd;
		Ctx.OutDebug->bEdgeOverheadBlocked = bEdgeBlocked;
		Ctx.OutDebug->EdgeOverheadClearance = bEdgeBlocked
			? (EdgeOverheadHit.ImpactPoint.Z - EdgeTop.Z) : Config.OverheadClearanceMargin;
		if (bEdgeBlocked) Ctx.OutDebug->EdgeOverheadHitPoint = EdgeOverheadHit.ImpactPoint;
	}
#endif

	if (bEdgeBlocked)
	{
		const float Headroom = EdgeOverheadHit.ImpactPoint.Z - EdgeTop.Z;
		Ctx.OutResult.RejectionReason = FString::Printf(
			TEXT("Overhead blocked at edge: %.0f cm (need %.0f)"), Headroom, Config.OverheadClearanceMargin);
		return false;
	}
	return true;
}

// --- Step 6: Landing overhead clearance ---
bool FOSMantleDetection::EvalStep_LandingOverhead(UWorld* World, FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	const FVector OverheadStart = Ctx.CapsuleCheckLocation + FVector(0, 0, Ctx.CharGeo.CapsuleHalfHeight);
	const FVector OverheadEnd = OverheadStart + FVector(0, 0, Config.OverheadClearanceMargin);

	FHitResult OverheadHit;
	const bool bOverheadHit = World->LineTraceSingleByChannel(
		OverheadHit, OverheadStart, OverheadEnd, Config.MantleTraceChannel, Ctx.OverheadParams);

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->bOverheadCheckRan = true;
		Ctx.OutDebug->OverheadTraceStart = OverheadStart;
		Ctx.OutDebug->OverheadTraceEnd = OverheadEnd;
		Ctx.OutDebug->bOverheadBlocked = bOverheadHit;
		Ctx.OutDebug->OverheadClearance = bOverheadHit
			? (OverheadHit.ImpactPoint.Z - OverheadStart.Z) : Config.OverheadClearanceMargin;
		if (bOverheadHit) Ctx.OutDebug->OverheadHitPoint = OverheadHit.ImpactPoint;
	}
#endif

	if (bOverheadHit)
	{
		const float Headroom = OverheadHit.ImpactPoint.Z - OverheadStart.Z;
		Ctx.OutResult.RejectionReason = FString::Printf(
			TEXT("Overhead blocked at landing: %.0f cm (need %.0f)"), Headroom, Config.OverheadClearanceMargin);
		return false;
	}
	return true;
}

// --- Step 7: Landing floor verification ---
bool FOSMantleDetection::EvalStep_LandingFloor(UWorld* World, FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	const FVector FloorTraceStart = Ctx.DownHit.ImpactPoint + FVector(0, 0, 20.f);
	const FVector FloorTraceEnd = Ctx.DownHit.ImpactPoint - FVector(0, 0, Ctx.CharGeo.CapsuleHalfHeight + 50.f);

	FHitResult FloorHit;
	bool bFloorHit = World->LineTraceSingleByChannel(
		FloorHit, FloorTraceStart, FloorTraceEnd, Config.MantleTraceChannel, Ctx.QueryParams);

	const float FloorAngle = bFloorHit
		? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(FloorHit.ImpactNormal, FVector::UpVector), -1.f, 1.f)))
		: 0.f;
	const bool bFloorWalkable = bFloorHit && (FloorHit.ImpactNormal.Z >= Ctx.WalkableFloorZ);

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->bFloorCheckRan = true;
		Ctx.OutDebug->FloorTraceStart = FloorTraceStart;
		Ctx.OutDebug->FloorTraceEnd = FloorTraceEnd;
		Ctx.OutDebug->bFloorHit = bFloorHit;
		Ctx.OutDebug->FloorAngle = FloorAngle;
		Ctx.OutDebug->bFloorWalkable = bFloorWalkable;
		if (bFloorHit)
		{
			Ctx.OutDebug->FloorHitPoint = FloorHit.ImpactPoint;
			Ctx.OutDebug->FloorHitNormal = FloorHit.ImpactNormal;
		}
	}
#endif

	// Store floor result but don't reject yet. Classification (step 8) determines
	// Vault vs Climb/Catch. Vaults land on the far side — the on-top floor is irrelevant.
	// For thin obstacles (fences), the on-top floor trace often hits a steep geometry
	// junction, falsely rejecting valid vault targets. Rejection is deferred to after
	// classification: Climb/Catch require walkable on-top floor, Vaults don't.
	Ctx.OutResult.Obstacle.bOnTopFloorFound = bFloorHit;
	Ctx.OutResult.Obstacle.bOnTopFloorWalkable = bFloorWalkable;
	return true;
}

// --- Step 8: Classification (vault / climb / catch) ---
void FOSMantleDetection::EvalStep_Classify(UWorld* World, FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	FOSObstacleGeo& ObsGeo = Ctx.OutResult.Obstacle;

	const FVector VaultEffNorm = ObsGeo.EffectiveNormal();
	const FVector VaultPastDir = FVector(-VaultEffNorm.X, -VaultEffNorm.Y, 0.f).GetSafeNormal();
	const FVector VaultProbeStart = Ctx.DownHit.ImpactPoint + FVector(0, 0, EdgeHeightOffset);
	const FVector VaultProbeEnd = VaultProbeStart + (VaultPastDir * Ctx.VaultDepth);

	FHitResult VaultForwardHit;
	const bool bVaultBlocked = World->LineTraceSingleByChannel(
		VaultForwardHit, VaultProbeStart, VaultProbeEnd, Config.MantleTraceChannel, Ctx.QueryParams);

	ObsGeo.bDepthProbed = true;
	ObsGeo.bFarSideOpen = !bVaultBlocked;
	ObsGeo.EstimatedDepth = bVaultBlocked ? (VaultForwardHit.ImpactPoint - VaultProbeStart).Size() : 0.f;

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->bVaultTraceRan = true;
		Ctx.OutDebug->VaultTraceStart = VaultProbeStart;
		Ctx.OutDebug->VaultTraceEnd = VaultProbeEnd;
		Ctx.OutDebug->bVaultFarSideClear = !bVaultBlocked;
	}
#endif

	bool bClassifiedAsVault = false;

	if (!bVaultBlocked)
	{
		const FVector VaultFloorStart = VaultProbeEnd;
		const FVector VaultFloorEnd = FVector(VaultProbeEnd.X, VaultProbeEnd.Y,
			Ctx.CharGeo.Feet.Z - Ctx.VaultDrop);

		FHitResult VaultFloorHit;
		const bool bVaultFloor = World->LineTraceSingleByChannel(
			VaultFloorHit, VaultFloorStart, VaultFloorEnd, Config.MantleTraceChannel, Ctx.QueryParams);

		const bool bVaultFloorWalkable = bVaultFloor && (VaultFloorHit.ImpactNormal.Z >= Ctx.WalkableFloorZ);

		ObsGeo.bFarGroundFound = bVaultFloor;
		if (bVaultFloor)
		{
			ObsGeo.FarGroundPoint = VaultFloorHit.ImpactPoint;
			ObsGeo.FarGroundNormal = VaultFloorHit.ImpactNormal;
			ObsGeo.bFarGroundWalkable = bVaultFloorWalkable;
			ObsGeo.FarDrop = Ctx.CharGeo.Feet.Z - VaultFloorHit.ImpactPoint.Z;
		}

#if !UE_BUILD_SHIPPING
		if (Ctx.OutDebug)
		{
			Ctx.OutDebug->VaultFloorStart = VaultFloorStart;
			Ctx.OutDebug->VaultFloorEnd = VaultFloorEnd;
			Ctx.OutDebug->bVaultFloorFound = bVaultFloorWalkable;
			if (bVaultFloor) Ctx.OutDebug->VaultFloorHitPoint = VaultFloorHit.ImpactPoint;
		}
#endif

		if (bVaultFloorWalkable)
		{
			const bool bSignificantDrop = ObsGeo.FarDrop > Ctx.CharGeo.KneeHeight();
			// Low/thin obstacle with clear far side and walkable ground = vault,
			// even if ground is at the same height or HIGHER (railing on raised platform).
			// WaistHeight limit ensures only short obstacles (fences, railings) get this
			// treatment. Taller walls (chest height+) should climb to land on top.
			const float FarGroundVsLedge = FMath::Abs(
				VaultFloorHit.ImpactPoint.Z - ObsGeo.LedgePoint.Z);
			const bool bLowFence = (FarGroundVsLedge < Ctx.CharGeo.KneeHeight())
				&& (ObsGeo.LedgeHeight <= Ctx.CharGeo.WaistHeight())
				&& ObsGeo.bFarSideOpen;

			if (bSignificantDrop || bLowFence)
			{
				const FVector VaultLandCenter = FVector(VaultFloorHit.ImpactPoint.X,
					VaultFloorHit.ImpactPoint.Y,
					VaultFloorHit.ImpactPoint.Z + Ctx.CharGeo.CapsuleHalfHeight);

				TArray<FHitResult> VaultLandOverlaps;
				World->SweepMultiByChannel(
					VaultLandOverlaps, VaultLandCenter, VaultLandCenter, FQuat::Identity,
					Config.MantleTraceChannel,
					FCollisionShape::MakeCapsule(Ctx.CharGeo.CapsuleRadius, Ctx.CharGeo.CapsuleHalfHeight),
					Ctx.QueryParams);

				bool bVaultLandingBlocked = false;
				for (const FHitResult& Overlap : VaultLandOverlaps)
				{
					if (Overlap.ImpactNormal.IsNearlyZero() || Overlap.ImpactNormal.Z < Ctx.WalkableFloorZ)
					{
						bVaultLandingBlocked = true;
						break;
					}
				}

				if (!bVaultLandingBlocked)
				{
					ObsGeo.Type = EOSMantleType::Vault;
					bClassifiedAsVault = true;
				}
			}
		}
		else if (!bVaultFloor)
		{
			// No far ground at all (cliff edge, void) — vault with gravity landing.
			// The character vaults over and gravity handles the descent.
			ObsGeo.Type = EOSMantleType::Vault;
			bClassifiedAsVault = true;
		}
		// else: far ground exists but isn't walkable (steep slope) — fall through to Climb.
		// Landing on top of the obstacle is safer than vaulting onto a steep slope.
	}

	if (!bClassifiedAsVault)
	{
		ObsGeo.Type = Ctx.CharGeo.bAirborne ? EOSMantleType::Catch : EOSMantleType::Climb;
	}

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->MantleType = ObsGeo.Type;
		Ctx.OutDebug->ObstGeo = ObsGeo;
	}
#endif
}

// --- Step 10: Intent filtering (jump vs mantle) ---
bool FOSMantleDetection::EvalStep_IntentFilter(FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	const FOSObstacleGeo& ObsGeo = Ctx.OutResult.Obstacle;

	if (ObsGeo.Type == EOSMantleType::Vault)
	{
		if (Config.MinApproachSpeed > 0.f)
		{
			const FVector IntentEffNorm = ObsGeo.EffectiveNormal();
			const FVector WallDir2D = FVector(-IntentEffNorm.X, -IntentEffNorm.Y, 0.f).GetSafeNormal();
			const float InputAlignment = FVector::DotProduct(Ctx.CharGeo.InputDirection, WallDir2D);
			const FVector Vel2D(Ctx.CharGeo.Velocity.X, Ctx.CharGeo.Velocity.Y, 0.f);
			const float ApproachSpeed = FVector::DotProduct(Vel2D, WallDir2D);

#if !UE_BUILD_SHIPPING
			if (Ctx.OutDebug)
			{
				Ctx.OutDebug->bIntentFilterRan = true;
				Ctx.OutDebug->IntentInputAlignment = InputAlignment;
				Ctx.OutDebug->IntentApproachSpeed = ApproachSpeed;
			}
#endif

			// Require EITHER input alignment (player pressing toward wall) OR
			// approach speed WITH neutral-or-positive input. This allows:
			// - Stationary vault: standing next to fence, press toward it + mantle
			// - Running vault: sprinting at fence, even without precise stick aim
			// Blocks: knockback into fence (speed without input toward wall)
			const bool bHasInputIntent = (InputAlignment >= Config.MinVaultInputAlignment);
			const bool bHasSpeedIntent = (ApproachSpeed >= Config.MinApproachSpeed);

			if (bHasInputIntent)
			{
				// Player pressing toward wall — vault regardless of speed (stationary OK)
			}
			else if (bHasSpeedIntent)
			{
				// Moving fast toward wall but stick not precisely aimed.
				// Block if player is actively pressing AWAY (knockback scenario).
				if (!Ctx.CharGeo.InputDirection.IsNearlyZero(0.1f) && InputAlignment < 0.f)
				{
					Ctx.OutResult.RejectionReason = FString::Printf(
						TEXT("Vault: speed toward wall but input away (align=%.2f)"), InputAlignment);
					UE_LOG(LogOSMantle, Verbose, TEXT("Intent: %s"), *Ctx.OutResult.RejectionReason);
					return false;
				}
			}
			else
			{
				Ctx.OutResult.RejectionReason = FString::Printf(
					TEXT("Vault: no intent (align=%.2f speed=%.0f)"), InputAlignment, ApproachSpeed);
				UE_LOG(LogOSMantle, Verbose, TEXT("Intent: %s"), *Ctx.OutResult.RejectionReason);
				return false;
			}
		}
	}
	else if (ObsGeo.Type == EOSMantleType::Climb)
	{
		// Climbs must be at least knee height. The minimum in FindLedge was lowered
		// to 20cm to allow short vault-height obstacles through classification.
		// Now that we know this is a Climb (not a Vault), enforce the taller minimum.
		if (ObsGeo.LedgeHeight < Ctx.CharGeo.KneeHeight())
		{
			Ctx.OutResult.RejectionReason = FString::Printf(
				TEXT("Climb below knee (%.0f cm < %.0f)"),
				ObsGeo.LedgeHeight, Ctx.CharGeo.KneeHeight());
			UE_LOG(LogOSMantle, Verbose, TEXT("Intent: %s"), *Ctx.OutResult.RejectionReason);
			return false;
		}

		if (Config.JumpClearanceFactor > 0.f && Ctx.CharGeo.MaxJumpHeight > 0.f)
		{
			const float ClearanceHeight = Ctx.CharGeo.MaxJumpHeight * Config.JumpClearanceFactor;

#if !UE_BUILD_SHIPPING
			if (Ctx.OutDebug)
			{
				Ctx.OutDebug->bIntentFilterRan = true;
				Ctx.OutDebug->IntentClearanceHeight = ClearanceHeight;
			}
#endif

			if (ObsGeo.LedgeHeight <= ClearanceHeight)
			{
				Ctx.OutResult.RejectionReason = FString::Printf(
					TEXT("Low climb (%.0f cm <= %.0f): jump preferred"),
					ObsGeo.LedgeHeight, ClearanceHeight);
				UE_LOG(LogOSMantle, Verbose, TEXT("Intent: %s"), *Ctx.OutResult.RejectionReason);
				return false;
			}
		}
	}

	return true;
}

// --- Step 11: Ledge width sampling ---
bool FOSMantleDetection::EvalStep_LedgeWidth(UWorld* World, FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	if (Config.LedgeSampleCount <= 1) return true;

	const FOSObstacleGeo& ObsGeo = Ctx.OutResult.Obstacle;
	const int32 EffectiveSampleCount = Config.LedgeSampleCount | 1;
	const FVector LedgeLateral = FVector::CrossProduct(ObsGeo.EffectiveNormal(), FVector::UpVector).GetSafeNormal();
	const int32 SidesPerSide = (EffectiveSampleCount - 1) / 2;
	const float SampleSpacing = Ctx.CharGeo.CapsuleRadius / FMath::Max(SidesPerSide, 1);

	int32 SamplesPassed = 1;
	int32 SamplesTotal = 1;

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->LedgeSamplePoints.Add(Ctx.DownHit.ImpactPoint);
		Ctx.OutDebug->LedgeSampleResults.Add(true);
	}
#endif

	for (int32 i = 1; i <= SidesPerSide; ++i)
	{
		const float Offset = SampleSpacing * i;
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const FVector SampleOffset = LedgeLateral * (Offset * Side);
			const FVector SampleStart = Ctx.DownTraceStart + SampleOffset;
			const FVector SampleEnd = Ctx.DownTraceEnd + SampleOffset;

			FHitResult SampleHit;
			const bool bSampleHit = World->LineTraceSingleByChannel(
				SampleHit, SampleStart, SampleEnd, Config.MantleTraceChannel, Ctx.QueryParams);

			++SamplesTotal;
			const bool bSampleOk = bSampleHit && SampleHit.ImpactNormal.Z >= Ctx.WalkableFloorZ;
			if (bSampleOk) ++SamplesPassed;

#if !UE_BUILD_SHIPPING
			if (Ctx.OutDebug)
			{
				Ctx.OutDebug->LedgeSamplePoints.Add(bSampleHit ? SampleHit.ImpactPoint : SampleEnd);
				Ctx.OutDebug->LedgeSampleResults.Add(bSampleOk);
			}
#endif
		}
	}

#if !UE_BUILD_SHIPPING
	if (Ctx.OutDebug)
	{
		Ctx.OutDebug->LedgeSamplesTotal = SamplesTotal;
		Ctx.OutDebug->LedgeSamplesPassed = SamplesPassed;
	}
#endif

	if (ObsGeo.Type != EOSMantleType::Vault)
	{
		const int32 MinRequired = FMath::CeilToInt(SamplesTotal * Config.LedgeSamplePassRatio);
		if (SamplesPassed < MinRequired)
		{
			Ctx.OutResult.RejectionReason = FString::Printf(TEXT("Narrow ledge: %d/%d"), SamplesPassed, SamplesTotal);
			return false;
		}
	}

	return true;
}

// --- Step 12: Deferred falling check (climb only) ---
bool FOSMantleDetection::EvalStep_DeferredFalling(FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	const FOSObstacleGeo& ObsGeo = Ctx.OutResult.Obstacle;
	if (ObsGeo.Type == EOSMantleType::Climb && Config.bRequireFalling)
	{
		if (!Ctx.CharGeo.bAirborne)
		{
			Ctx.OutResult.RejectionReason = TEXT("Climb requires jumping");
			return false;
		}
		if (Ctx.CharGeo.VerticalVelocity < Config.MinVerticalVelocity)
		{
			Ctx.OutResult.RejectionReason = FString::Printf(TEXT("Climb velocity %.0f < %.0f"),
				Ctx.CharGeo.VerticalVelocity, Config.MinVerticalVelocity);
			return false;
		}
	}
	return true;
}

// --- Step 13: Warp delta pre-check ---
bool FOSMantleDetection::EvalStep_WarpDelta(FEvalCtx& Ctx)
{
	const auto& Config = Ctx.Config;
	if (Config.MaxEdgeWarpDelta > 0.f)
	{
		const FOSObstacleGeo& ObsGeo = Ctx.OutResult.Obstacle;
		const float VerticalDelta = FMath::Abs((ObsGeo.LedgePoint.Z + 5.f) - Ctx.CharGeo.Feet.Z);
		if (VerticalDelta > Config.MaxEdgeWarpDelta)
		{
			Ctx.OutResult.RejectionReason = FString::Printf(
				TEXT("Estimated post-approach delta %.0f > max %.0f"), VerticalDelta, Config.MaxEdgeWarpDelta);
			return false;
		}
	}
	return true;
}

// ========================================
// CANDIDATE SCORING
// ========================================
// ========================================
// CANDIDATE SCORING
// ========================================

float FOSMantleDetection::ScoreCandidate(const FOSCharacterGeo& CharGeo,
    const FOSMantleTraceResult& Result, float DetectionRadius,
    const FOSMantleDetectionConfig& Config)
{
    const FOSObstacleGeo& Obs = Result.Obstacle;

    // Distance: closer = higher (0-1)
    const float Dist2D = FVector::Dist2D(CharGeo.Feet, Obs.WallPoint);
    const float DistScore = FMath::Clamp(1.f - (Dist2D / FMath::Max(DetectionRadius, 1.f)), 0.f, 1.f);

    // Angle: facing wall directly = higher (0-1).
    // Uses EffectiveNormal (profile consensus) so inset geometry scores by the
    // actual face the character will interact with, not the protruding base.
    const FVector EffNormal = Obs.EffectiveNormal();
    const float FacingDot = FVector::DotProduct(CharGeo.Forward, -EffNormal);
    const float AngleScore = FMath::Clamp(FacingDot, 0.f, 1.f);

    // Warp delta: shorter MW travel = higher (0-1)
    // Edge target is at wall face at ledge height, not behind the wall face
    const float EdgeDelta = FVector::Dist(CharGeo.Feet, ComputeEdgePosition(Obs));
    const float MaxDelta = (Config.MaxEdgeWarpDelta > 0.f) ? Config.MaxEdgeWarpDelta : (Config.MaxMantleHeight + CharGeo.CapsuleRadius);
    const float WarpScore = FMath::Clamp(1.f - (EdgeDelta / FMath::Max(MaxDelta, 1.f)), 0.f, 1.f);

    // Height: peak at chest height, falloff above/below (0-1)
    const float HeightDiff = FMath::Abs(Obs.LedgeHeight - CharGeo.ChestHeight());
    const float HeightScore = FMath::Clamp(1.f - (HeightDiff / FMath::Max(Config.MaxMantleHeight, 1.f)), 0.f, 1.f);

    const float TotalWeight = Config.ScoreWeightDistance + Config.ScoreWeightAngle + Config.ScoreWeightWarpDelta + Config.ScoreWeightHeight;
    if (TotalWeight <= 0.f) return 0.f;

    const float BaseScore = (DistScore * Config.ScoreWeightDistance
          + AngleScore * Config.ScoreWeightAngle
          + WarpScore * Config.ScoreWeightWarpDelta
          + HeightScore * Config.ScoreWeightHeight) / TotalWeight;

    // Profile coverage: wider wall faces score higher than narrow elements.
    // 0% → 0.25x, 67% → 1.0x, 100% → 1.0x. Gentle penalty that deprioritizes
    // narrow candidates (stilts, posts) without hard-rejecting valid geometry
    // (leaning walls, decorative trim that only hits 1-2 slices).
    const float CoverageFactor = Obs.bProfileRan
        ? FMath::Clamp(Obs.ProfileHitRatio * 1.5f, 0.5f, 1.0f) // Floor raised from 0.25 to 0.5
        : 1.0f;

    return BaseScore * CoverageFactor;
}
