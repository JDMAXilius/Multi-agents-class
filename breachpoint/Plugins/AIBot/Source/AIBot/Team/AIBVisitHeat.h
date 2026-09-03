#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

/**
 * PHASE 12 (AIB23) — the TEAM VISIT HEAT GRID, the Team Mind's first member (deferred here
 * from Phase 11 step 6). A coarse cube grid of the team's OWN footsteps, decayed: Roam's
 * wander walks the coldest of a few draws, so four bots explore the level instead of
 * re-treading one corner. TEAM-ONLY by law (AIB22 critic): a stamp is readable only by
 * the stamper's allies — an enemy's footsteps are perception, never a grid read (F3).
 * Headless: time and cell size are parameters, alliance is injected, pawns are handles.
 */
struct AIBOT_API FAIBVisitHeat
{
	using FAreAllies = TFunctionRef<bool(const AActor*, const AActor*)>;

	struct FStamp
	{
		FObjectKey Visitor;
		TWeakObjectPtr<const AActor> VisitorPawn;
		double AtSeconds = 0.0;
	};

	static FIntVector CellOf(const FVector& Where, float CellUU);

	/** One stamp per visitor per cell; the newest wins. */
	void Stamp(FObjectKey Visitor, const AActor* VisitorPawn, const FVector& Where, double Now, float CellUU);

	/** 0..1: exp(−age/Decay) of the freshest ALLIED stamp (self included) in Where's cell;
	 *  0 for a cell no teammate ever visited. */
	float HeatAt(FObjectKey Asker, const AActor* AskerPawn, const FVector& Where, double Now,
		float CellUU, float DecaySeconds, FAreAllies AreAllies) const;

	/** Drops stamps too old to read above ~2 % (4 decay constants) and empty cells. */
	void Prune(double Now, float DecaySeconds);

	TMap<FIntVector, TArray<FStamp>> Cells;
};
