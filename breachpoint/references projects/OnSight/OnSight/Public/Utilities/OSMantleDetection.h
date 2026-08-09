// OSMantleDetection.h
// Stateless mantle detection engine.
// Performs spatial queries to find and evaluate mantle candidates.
// Called by GA_OSmantleability but has no dependency on GAS.
//
// Detection pipeline:
//   1. Overlap sphere finds all geometry within range
//   2. Per-component targeted trace finds wall faces
//   3. Each wall face is evaluated (ledge, clearance, overhead, classification)
//   4. Valid candidates are scored (distance, angle, warp delta, height)
//   5. Best candidate is chosen

#pragma once

#include "CoreMinimal.h"
#include "Data/OSMantleTypes.h"

class ACharacter;

// Configuration for the detection engine.
// Populated from the ability's UPROPERTYs before each detection call.
struct FOSMantleDetectionConfig
{
    // --- Detection ---
    float MaxMantleHeight = 250.f;
    float DetectionSphereRadius = 0.f;
    float MaxDetectionAngle = 120.f;
    int32 ProfileSliceCount = 6;
    float InsetDetectionThreshold = 10.f;
    int32 MaxCandidates = 5;
    float SweepTraceRadius = 5.f;
    TEnumAsByte<ECollisionChannel> MantleTraceChannel = ECC_Visibility;
    float MaxSurfaceAngle = 0.f;
    float OverheadClearanceMargin = 30.f;
    bool bRequireFalling = false;
    float MinVerticalVelocity = -100.f;

    // --- Arbitration ---
    float JumpClearanceFactor = 0.4f;
    float MinApproachSpeed = 150.f;
    float MinVaultInputAlignment = 0.5f;

    // --- Scoring ---
    float ScoreWeightDistance = 0.3f;
    float ScoreWeightAngle = 0.3f;
    float ScoreWeightWarpDelta = 0.25f;
    float ScoreWeightHeight = 0.15f;

    // --- Physics ---
    float VelocityTraceBlend = 0.5f;
    float VelocityBlendMinSpeed = 50.f;
    int32 LedgeSampleCount = 3;
    float LedgeSamplePassRatio = 0.5f;
    float GravityLookaheadTime = 0.1f;

    // --- Motion Warping ---
    float MaxEdgeWarpDelta = 300.f;
    float MaxLandingWarpDelta = 100.f;
};

// Stateless mantle detection utility.
// All methods are static — no instance state, no UObject, no reflection.
class FOSMantleDetection
{
public:
    // Build character geometry profile from a character actor.
    static FOSCharacterGeo BuildCharacterGeo(
        const ACharacter* Character,
        const FOSMantleDetectionConfig& Config);

    // Run the full detection pipeline: overlap -> trace -> evaluate -> score -> pick best.
    static FOSMantleTraceResult PerformTrace(
        UWorld* World,
        const ACharacter* Character,
        const FOSCharacterGeo& CharGeo,
        const FOSMantleDetectionConfig& Config
#if !UE_BUILD_SHIPPING
        , FOSMantleDebugData* OutDebug = nullptr
#endif
    );

    // Height offset for edge warp target (cm above ledge surface).
    static constexpr float EdgeHeightOffset = 5.f;

    // Edge warp target position: wall face XY at ledge height + offset.
    static FVector ComputeEdgePosition(const FOSObstacleGeo& Obs);

private:
    // --- Evaluation pipeline context (defined in .cpp) ---
    struct FEvalCtx;

    // Evaluate a single wall hit through the full pipeline.
    static bool EvaluateWallHit(
        UWorld* World,
        const FOSCharacterGeo& CharGeo,
        const FHitResult& WallHit,
        float WalkableFloorZ,
        const FCollisionQueryParams& QueryParams,
        const FOSMantleDetectionConfig& Config,
        FOSMantleTraceResult& OutResult
#if !UE_BUILD_SHIPPING
        , FOSMantleDebugData* OutDebug = nullptr
#endif
    );

    // Pipeline steps
    static bool EvalStep_InitAndProfile(UWorld* World, FEvalCtx& Ctx);
    static bool EvalStep_FindLedge(UWorld* World, FEvalCtx& Ctx);
    static bool EvalStep_CapsuleClearance(UWorld* World, FEvalCtx& Ctx);
    static bool EvalStep_ApproachPath(UWorld* World, FEvalCtx& Ctx);
    static bool EvalStep_EdgeOverhead(UWorld* World, FEvalCtx& Ctx);
    static bool EvalStep_LandingOverhead(UWorld* World, FEvalCtx& Ctx);
    static bool EvalStep_LandingFloor(UWorld* World, FEvalCtx& Ctx);
    static void EvalStep_Classify(UWorld* World, FEvalCtx& Ctx);
    static bool EvalStep_IntentFilter(FEvalCtx& Ctx);
    static bool EvalStep_LedgeWidth(UWorld* World, FEvalCtx& Ctx);
    static bool EvalStep_DeferredFalling(FEvalCtx& Ctx);
    static bool EvalStep_WarpDelta(FEvalCtx& Ctx);

    // Build vertical wall profile.
    static bool ProfileWall(
        UWorld* World,
        const FOSCharacterGeo& CharGeo,
        const FVector& TraceDirection,
        float TraceLength,
        AActor* WallActor,
        const FCollisionQueryParams& QueryParams,
        const FOSMantleDetectionConfig& Config,
        FOSObstacleGeo& OutObs
#if !UE_BUILD_SHIPPING
        , FOSMantleDebugData* OutDebug = nullptr
#endif
    );

    // Score a validated candidate.
    static float ScoreCandidate(
        const FOSCharacterGeo& CharGeo,
        const FOSMantleTraceResult& Result,
        float DetectionRadius,
        const FOSMantleDetectionConfig& Config);
};
