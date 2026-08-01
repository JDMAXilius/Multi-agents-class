// Breachpoint. Layer 2's spatial vocabulary: EQS contexts over the arena's AUTHORED places.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"

#include "BREnvQueryContexts.generated.h"

class AActor;
class APawn;
class UWorld;

/**
 * BREnvQueryContexts — every "where" a bot can ask about, and NOTHING ELSE.
 *
 * Halo lesson 2, which AI-BOTS §4 makes binding: space is AUTHORED, not inferred. Bots
 * fight about places a designer named — the Core, the Mezzanine, a chest-high stack — and
 * `Content/Data/arena_manifest.json` is that vocabulary. These contexts are the only bridge
 * between a plan step's abstract EBRBotAnchor and a coordinate in the level.
 *
 * WHAT THIS BUYS, beyond taste: callouts come free ("contesting the Pad" is the landmark's
 * own name), the tuning-curator can reason about places, and a bot cannot exploit geometry
 * a designer never blessed. Raw nav-mesh divination — "find any point 400 uu from the enemy"
 * — is explicitly not how this game's bots choose ground.
 *
 * THE SCORING lives in the EQS query assets (Tier 4, authored under Content/AI/), because
 * UE has no C++ path for a query graph. These contexts only PROVIDE the candidate sets; the
 * tests (distance, LoS/trace, dot) and their weights are the asset's business. That split is
 * deliberate: it keeps scoring numbers out of C++ exactly as law 3 requires.
 */

// =============================================================================================
// The arena vocabulary lookup
// =============================================================================================

/**
 * How C++ finds the manifest's places in a loaded level.
 *
 * CONTRACT GAP (BP08 contract_gap 3): nothing in Source/ reads `arena_manifest.json` today,
 * and the BP07 blockout script does not (yet) tag what it places. Until it does, these
 * lookups find nothing, and every EQS query degrades to "no result" -> the step fails ->
 * the brain replans. That is the correct failure: a bot that stands still is a bug report,
 * a bot that invents a destination is a mystery.
 *
 * The fix is one line in the blockout generator: tag each landmark/cover/perch actor with
 * the FNames below (or land a manifest accessor and let this namespace call it instead).
 */
namespace BRBotArenaTags
{
	/** Every actor generated from `landmarks[]`. */
	BREACHPOINT_API extern const FName Landmark;
	/** The power-weapon pad landmark ("The Core"). Also carries Landmark. */
	BREACHPOINT_API extern const FName RocketPad;
	/** Every actor generated from `cover[]`. */
	BREACHPOINT_API extern const FName Cover;
	/** Grapple perches — the subset of landmarks a grapple can reach and stand on. */
	BREACHPOINT_API extern const FName Perch;
}

namespace BRBotArena
{
	/** All actors carrying an arena vocabulary tag. Empty until BP08 contract_gap 3 closes. */
	BREACHPOINT_API void GatherTagged(const UWorld* World, FName Tag, TArray<AActor*>& OutActors);

	/** The rocket pad's location. False when the vocabulary is missing. */
	BREACHPOINT_API bool GetRocketPadLocation(const UWorld* World, FVector& OutLocation);

	/**
	 * Quality of the cover this pawn currently occupies, in [0, 1].
	 *
	 * DERIVED GEOMETRY, NOT TUNING: it is the occupied cover's height over the pawn's own
	 * height, clamped — full cover reads 1, chest-high reads whatever the blockout actually
	 * built, nothing reads a number someone typed in this file. 0 when standing in the open.
	 */
	BREACHPOINT_API float GetCoverQualityAt(const APawn* Pawn);
}

// =============================================================================================
// Contexts
// =============================================================================================

/** The bot's current perceived target. Empty when it has none — queries then produce no result. */
UCLASS()
class BREACHPOINT_API UBREnvQueryContext_Threat : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/** The power-weapon pad. The one place the whole match is organised around. */
UCLASS()
class BREACHPOINT_API UBREnvQueryContext_RocketPad : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/** Every authored cover point (`cover[]`). Queries score them against the threat context. */
UCLASS()
class BREACHPOINT_API UBREnvQueryContext_CoverPoints : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/** Grapple perches. The reason a bot's vertical play reads as intentional rather than lucky. */
UCLASS()
class BREACHPOINT_API UBREnvQueryContext_GrapplePerches : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/** Living teammates, for the Regroup step. Team attitude comes from the controller, not a scan of PlayerStates. */
UCLASS()
class BREACHPOINT_API UBREnvQueryContext_Teammates : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/**
 * THE anchor of the current plan step, whatever the brain asked for.
 *
 * This is what lets ST_Bot hold ONE MoveTo query instead of one per anchor kind: the query
 * asks "where is the step pointing", and layer 1's decision answers. Add an EBRBotAnchor and
 * every existing query follows it — which is the point of an authored vocabulary.
 */
UCLASS()
class BREACHPOINT_API UBREnvQueryContext_StepAnchor : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
