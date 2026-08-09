// OSMantleTypes.h
// Shared data types for the OnSight mantle system.
// Used by the mantle ability, detection engine, and debug HUD.

#pragma once

#include "CoreMinimal.h"
#include "OSMantleTypes.generated.h"

// Vault:  go OVER a thin obstacle, land on far side. Never stand on top.
// Climb:  go UP onto an obstacle from below or in front. Land on top. Grounded only.
// Catch:  AIRBORNE, grab ledge on the way past. Pull up onto top.
UENUM(BlueprintType)
enum class EOSMantleType : uint8
{
    Vault,
    Climb,
    Catch
};

// ============================================================================
// Character geometry profile -- snapshot of the character's physical dimensions
// and movement state at trace time. All detection thresholds are derived from this.
// ============================================================================
struct FOSCharacterGeo
{
    // Capsule dimensions
    float CapsuleRadius = 0.f;
    float CapsuleHalfHeight = 0.f;
    float FullHeight = 0.f;       // 2 * CapsuleHalfHeight

    // Positions (at trace time, may include gravity prediction)
    FVector Feet = FVector::ZeroVector;       // capsule base (actor location)
    FVector Center = FVector::ZeroVector;     // capsule center (feet + half height)

    // Movement state
    FVector Forward = FVector::ForwardVector; // blended trace direction (velocity + actor forward)
    FVector Velocity = FVector::ZeroVector;
    bool bAirborne = false;
    float VerticalVelocity = 0.f;

    // CMC walkability
    float WalkableFloorZ = 0.f;

    // Max jump apex height (derived from CMC: JumpZVelocity² / 2*|gravity|)
    float MaxJumpHeight = 0.f;

    // Pure player input direction (excludes knockback/root motion/physics forces).
    // Populated via UOSCombatBlueprintLibrary::GetInputDirection2D().
    FVector InputDirection = FVector::ZeroVector;

    // --- Procedural thresholds (derived from capsule) ---

    float KneeHeight() const   { return FullHeight * 0.28f; }   // ~49cm for 176cm capsule
    float WaistHeight() const  { return CapsuleHalfHeight; }    // ~88cm -- capsule center
    float ChestHeight() const  { return FullHeight * 0.72f; }   // ~127cm for 176cm capsule
    float Diameter() const     { return CapsuleRadius * 2.f; }

    // How far past the wall to start the down trace -- must clear wall face + allow capsule check
    float DownTraceForwardOffset() const { return CapsuleRadius + 5.f; }

    // How thick an obstacle can be and still count as vaultable
    float MaxVaultableDepth() const { return FullHeight * 0.6f; }

    // Max safe drop on far side of a vault -- roughly character height
    float MaxVaultDrop() const { return FullHeight; }

    // How high above a hit point to start searching for ledge
    float DownTraceSearchHeight(float MaxMantleHeight) const { return MaxMantleHeight + CapsuleHalfHeight; }

    // Pull-over distance -- place capsule center past the ledge edge
    float IdealPullOver() const { return CapsuleRadius + 10.f; }
    // Minimum pull-over: 0 allows the character to land exactly at the ledge edge.
    // The iterative capsule clearance check starts at IdealPullOver and reduces to this.
    // Previously CapsuleRadius * 0.5f (17cm) — forced the capsule past the edge, which
    // caused collisions with inset/recessed wall geometry below the ledge.
    float MinPullOver() const   { return 0.f; }
};

// ============================================================================
// Obstacle geometry profile -- built incrementally by successive traces.
// Captures the complete geometry context of the obstacle being mantled.
// ============================================================================
struct FOSObstacleGeo
{
    // --- Phase 1: Wall face (forward trace) ---
    bool bWallFound = false;
    FVector WallPoint = FVector::ZeroVector;    // impact point on wall face
    FVector WallNormal = FVector::ZeroVector;   // wall normal (points toward character)
    float WallAngle = 0.f;                      // angle from vertical

    // --- Phase 2: Ledge surface (down trace) ---
    bool bLedgeFound = false;
    FVector LedgePoint = FVector::ZeroVector;   // top surface of obstacle
    FVector LedgeNormal = FVector::ZeroVector;  // surface normal (should be ~up for walkable)
    float LedgeHeight = 0.f;                    // height above character feet (negative = ledge below feet)
    float SurfaceAngle = 0.f;                   // angle from up

    // --- Phase 2b: Wall normal at ledge height (re-sample) ---
    // The forward trace hits the wall at shin-to-waist height. For inset/recessed geometry,
    // the wall face may be at a different position at ledge height. Re-sampling gives the
    // actual wall normal where pull-over occurs — critical for correct capsule placement.
    bool bLedgeWallSampled = false;
    FVector LedgeWallPoint = FVector::ZeroVector;   // wall impact point at ledge height
    FVector LedgeWallNormal = FVector::ZeroVector;  // wall normal at ledge height

    // Effective wall normal for pull-over, approach, and warp direction.
    // LedgeWallNormal if re-sample succeeded, otherwise WallNormal.
    FVector EffectiveNormal() const { return bLedgeWallSampled ? LedgeWallNormal : WallNormal; }

    // --- Profile-derived geometry (populated by ProfileWall in EvaluateWallHit) ---
    bool bProfileRan = false;                   // true if ProfileWall executed (even with 0 hits)
    float ProfileHitRatio = 0.f;                // fraction of profile slices that hit (0.0-1.0)
    float WallTopHeight = 0.f;                  // height of wall top above character feet (from profile)
    bool bInsetDetected = false;                // distance variance across profile exceeds threshold
    float InsetDepth = 0.f;                     // max distance difference between profile slices (cm)

    // --- Phase 3: Obstacle depth (forward probe from ledge) ---
    bool bDepthProbed = false;
    bool bFarSideOpen = false;                  // no geometry blocking far side = thin obstacle
    float EstimatedDepth = 0.f;                 // distance from front face to back face (0 = open/thin)

    // --- Phase 4: Far side ground ---
    bool bFarGroundFound = false;
    FVector FarGroundPoint = FVector::ZeroVector;
    FVector FarGroundNormal = FVector::ZeroVector;
    bool bFarGroundWalkable = false;
    float FarDrop = 0.f;                        // character feet Z - far ground Z (positive = drop down)

    // --- Landing floor (on-top, from EvalStep_LandingFloor) ---
    // Deferred: Climb/Catch require walkable on-top floor. Vaults skip this check
    // because they land on the far side, not on top.
    bool bOnTopFloorFound = false;
    bool bOnTopFloorWalkable = false;

    // --- Classification (derived from above) ---
    EOSMantleType Type = EOSMantleType::Climb;
};

USTRUCT(BlueprintType)
struct FOSMantleTraceResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bSuccess = false;

    // Full geometry profiles
    FOSCharacterGeo Character;
    FOSObstacleGeo Obstacle;

    // Validated pull-over distance from capsule clearance
    float ValidatedPullOver = 0.0f;

    // Candidate score (higher = better)
    float Score = 0.f;

    // Rejection reason (empty if success)
    FString RejectionReason;
};

#if !UE_BUILD_SHIPPING
/** Per-candidate debug info for the HUD. */
struct FOSMantleCandidateDebug
{
    FVector WallPoint = FVector::ZeroVector;
    FVector WallNormal = FVector::ZeroVector;
    float Score = 0.f;
    bool bPassed = false;
    bool bChosen = false;
    FString RejectionReason;
};

/** Debug-only struct capturing all intermediate trace data for HUD visualization. */
struct FOSMantleDebugData
{
    // --- Geometry profiles (semantic data, from chosen candidate) ---
    FOSCharacterGeo CharGeo;
    FOSObstacleGeo ObstGeo;

    // --- Derived thresholds (for HUD display) ---
    float DerivedMinHeight = 0.f;         // knee height
    float DerivedHighThreshold = 0.f;     // waist height -- climb low/high + catch low/high split
    float DerivedJumpClearanceHeight = 0.f; // MaxJumpHeight * JumpClearanceFactor — climb/jump boundary
    float DerivedMaxVaultDepth = 0.f;     // max obstacle depth for vault classification
    float DerivedMaxVaultDrop = 0.f;      // max far-side drop for vault
    float DerivedIdealPullOver = 0.f;     // ideal landing distance past edge
    float DerivedDownTraceOffset = 0.f;   // how far past wall face for down trace
    float MaxMantleHeightUsed = 0.f;      // gameplay cap (from UPROPERTY)
    float MinApproachSpeedUsed = 0.f;     // vault approach speed threshold (from UPROPERTY)
    float MinVaultInputAlignmentUsed = 0.f; // vault input alignment threshold (from UPROPERTY)

    // --- Intent filtering debug ---
    bool bIntentFilterRan = false;          // true if intent filter was reached (post-classification)
    float IntentInputAlignment = 0.f;       // dot(InputDirection, WallToward) — vault gate 1
    float IntentApproachSpeed = 0.f;        // dot(Velocity2D, WallToward) — vault gate 2
    float IntentClearanceHeight = 0.f;      // MaxJumpHeight * JumpClearanceFactor — climb gate

    // --- Velocity-relative trace direction ---
    FVector TraceDirection = FVector::ZeroVector;
    FVector VelocityDirection = FVector::ZeroVector;
    float VelocityBlendUsed = 0.f;

    // --- Gravity prediction ---
    FVector PredictedLocation = FVector::ZeroVector;
    FVector ActualLocation = FVector::ZeroVector;
    bool bUsingPrediction = false;

    // --- Candidate debug (populated on demand, not currently visualized) ---
    TArray<FOSMantleCandidateDebug> Candidates;

    // --- Forward trace (from chosen candidate) ---
    FVector ForwardStart = FVector::ZeroVector;
    FVector ForwardEnd = FVector::ZeroVector;
    FVector ForwardHitPoint = FVector::ZeroVector;
    FVector ForwardHitNormal = FVector::ZeroVector;
    float ForwardHitAngle = 0.f;
    bool bForwardHit = false;

    // --- Downward trace ---
    FVector DownStart = FVector::ZeroVector;
    FVector DownEnd = FVector::ZeroVector;
    FVector DownHitPoint = FVector::ZeroVector;
    FVector DownHitNormal = FVector::ZeroVector;
    bool bDownHit = false;

    // --- Vertical wall profile (from EvaluateWallHit) ---
    struct FOSProfileSliceDebug
    {
        float Height = 0.f;
        bool bHit = false;
        FVector HitPoint = FVector::ZeroVector;
        FVector HitNormal = FVector::ZeroVector;
        float Distance = 0.f;
    };
    TArray<FOSProfileSliceDebug> ProfileSlices;
    FVector ProfileTraceDirection = FVector::ZeroVector;
    int32 ProfileWallTopIndex = INDEX_NONE;
    bool bProfileRan = false;
    bool bProfileInsetDetected = false;
    float ProfileInsetDepth = 0.f;
    FVector ProfileConsensusNormal = FVector::ZeroVector;

    // --- Multi-point ledge sampling ---
    int32 LedgeSamplesTotal = 0;
    int32 LedgeSamplesPassed = 0;
    TArray<FVector> LedgeSamplePoints;
    TArray<bool> LedgeSampleResults;

    // --- Capsule clearance ---
    FVector CapsuleCheckCenter = FVector::ZeroVector;
    float CapsuleRadius = 0.f;
    float CapsuleHalfHeight = 0.f;
    bool bCapsuleBlocked = false;
    int32 ClearanceOverlapTotal = 0;
    int32 ClearanceWalkableFiltered = 0;
    FString BlockerDetails;

    // --- Overhead clearance (ceiling/overhang check) ---
    // Approach overhead (check climb path from below — detects overhangs)
    FVector ApproachOverheadStart = FVector::ZeroVector;
    FVector ApproachOverheadEnd = FVector::ZeroVector;
    bool bApproachOverheadCheckRan = false;
    bool bApproachOverheadBlocked = false;
    FVector ApproachOverheadHitPoint = FVector::ZeroVector;
    // Edge overhead (check at ledge edge where character passes during climb)
    FVector EdgeOverheadStart = FVector::ZeroVector;
    FVector EdgeOverheadEnd = FVector::ZeroVector;
    bool bEdgeOverheadCheckRan = false;
    bool bEdgeOverheadBlocked = false;
    FVector EdgeOverheadHitPoint = FVector::ZeroVector;
    float EdgeOverheadClearance = 0.f;
    // Landing overhead (check at pull-over landing position)
    FVector OverheadTraceStart = FVector::ZeroVector;
    FVector OverheadTraceEnd = FVector::ZeroVector;
    bool bOverheadCheckRan = false;
    bool bOverheadBlocked = false;
    FVector OverheadHitPoint = FVector::ZeroVector;
    float OverheadClearance = 0.f;

    // --- Down trace ---
    float DownSweepRadius = 0.f;  // sphere sweep radius used for ledge detection

    // --- Landing floor verification ---
    FVector FloorTraceStart = FVector::ZeroVector;
    FVector FloorTraceEnd = FVector::ZeroVector;
    FVector FloorHitPoint = FVector::ZeroVector;
    FVector FloorHitNormal = FVector::ZeroVector;
    float FloorAngle = 0.f;
    bool bFloorHit = false;
    bool bFloorWalkable = false;
    bool bFloorCheckRan = false;

    // --- Edge warp target ---
    FVector EdgeWarpLocation = FVector::ZeroVector;
    FRotator EdgeWarpRotation = FRotator::ZeroRotator;
    bool bHasEdgeWarp = false;
    float EdgeWarpDelta = 0.f;

    // --- Landing warp target ---
    FVector LandingWarpLocation = FVector::ZeroVector;
    FRotator LandingWarpRotation = FRotator::ZeroRotator;
    bool bHasLandingWarp = false;
    float LandingWarpDelta = 0.f;
    float RotationDelta = 0.f;

    // --- Vault detection traces ---
    FVector VaultTraceStart = FVector::ZeroVector;
    FVector VaultTraceEnd = FVector::ZeroVector;
    FVector VaultFloorStart = FVector::ZeroVector;
    FVector VaultFloorEnd = FVector::ZeroVector;
    FVector VaultFloorHitPoint = FVector::ZeroVector;
    bool bVaultTraceRan = false;
    bool bVaultFarSideClear = false;
    bool bVaultFloorFound = false;
    EOSMantleType MantleType = EOSMantleType::Climb;

    // --- Geometry context ---
    float WalkableFloorZ = 0.f;
    float WalkableFloorAngle = 0.f;

    // --- Result ---
    float MantleHeight = 0.f;
    float SurfaceAngle = 0.f;
    FString RejectionReason;
    bool bSuccess = false;
};
#endif // !UE_BUILD_SHIPPING
