#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

namespace AIB
{
	/** Phase 14: lane area classes are a uint8 area-id budget; see UAIBNavArea_Lane. */
	inline constexpr int32 MaxRouteLanes = 6;
}

/** Phase 14 — ONE BOT'S ROUTE TASTE, drawn once per life. Six lane travel costs the query
 *  filter installs per query, so two bots with the same belief and the same A* still take
 *  different corridors. Worldless on purpose (the spec drives it with plain ints):
 *  - the LIFE SEED is HashCombine(HashCombine(MatchSeed, BotIndex), LifeIndex) — the match
 *    seed from the manager (-AIBSeed or the host), the bot's stable spawn slot, the life
 *    count — so two -AIBSeed=N runs draw byte-identical lanes for every bot and every life;
 *  - weights are 1 + U(-Spread, +Spread) per lane, then NORMALISED so the cheapest lane
 *    costs exactly 1 and every other ≥ 1: Recast's A* heuristic assumes area costs ≥ 1 (the
 *    engine's own override clamps at 1), and a sub-1 cost would make paths inadmissible
 *    rather than merely different. The relative preference is what the draw carries. */
struct AIBOT_API FAIBRouteBias
{
	float Costs[AIB::MaxRouteLanes] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	uint32 Seed = 0;

	static uint32 LifeSeed(int32 MatchSeed, int32 BotIndex, int32 LifeIndex)
	{
		return HashCombine(HashCombine(GetTypeHash(MatchSeed), GetTypeHash(BotIndex)), GetTypeHash(LifeIndex));
	}

	/** Spread is the row's RouteLaneWeightSpread, clamped to [0, 0.9] so no weight touches 0. */
	void Draw(uint32 InLifeSeed, float Spread)
	{
		Seed = InLifeSeed;
		const float S = FMath::Clamp(Spread, 0.f, 0.9f);
		// The module's prime pattern: one stream per subsystem off the life seed, so a
		// lane redraw can never shift a reaction latency or an aim error.
		FRandomStream Rng(static_cast<int32>(HashCombine(InLifeSeed, 2311u)));
		float MinWeight = FLT_MAX;
		for (float& Cost : Costs)
		{
			Cost = 1.f + Rng.FRandRange(-S, S);
			MinWeight = FMath::Min(MinWeight, Cost);
		}
		for (float& Cost : Costs)
		{
			Cost = FMath::Max(Cost / MinWeight, 1.f);
		}
	}

	/** Travel cost for a 1-based lane; 1 (the default area's cost) for anything else. */
	float CostOf(int32 LaneId) const
	{
		return LaneId >= 1 && LaneId <= AIB::MaxRouteLanes ? Costs[LaneId - 1] : 1.f;
	}

	/** "1:1.00,2:1.23,...": the `route bias` line's payload, the replay diff's evidence. */
	FString Describe() const
	{
		FString Out;
		for (int32 Lane = 1; Lane <= AIB::MaxRouteLanes; ++Lane)
		{
			Out += FString::Printf(TEXT("%s%d:%.2f"), Lane > 1 ? TEXT(",") : TEXT(""), Lane, Costs[Lane - 1]);
		}
		return Out;
	}
};
