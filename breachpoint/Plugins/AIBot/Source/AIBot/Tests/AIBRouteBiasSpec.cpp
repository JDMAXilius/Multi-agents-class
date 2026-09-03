#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBNavArea_Lane.h"
#include "Core/AIBRouteBias.h"
#include "Data/AIBDataRows.h"

/**
 * Phase 14 (AIB25), proven headless. The per-bot route taste is a pure function of the
 * seed triple: the same (match, bot, life) draws the same costs (1); a different bot,
 * life, or match draws different ones — no two bots in lockstep (2); the cheapest lane
 * costs exactly 1 and nothing is ever cheaper, so Recast's heuristic stays admissible (3);
 * the spread is a real knob — 0 is the conga line, and a wild value clamps instead of
 * zeroing a lane (4); a non-lane asks for the default cost, never 0 (5); the six lane
 * classes round-trip their ordinal and nothing else is a lane (6); the row default is what
 * the csv must mirror (7); and the `route bias` payload is stable text (8).
 */
BEGIN_DEFINE_SPEC(FAIBRouteBiasSpec, "AIBot.Sim.RouteBias",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static bool SameCosts(const FAIBRouteBias& A, const FAIBRouteBias& B)
	{
		for (int32 Lane = 0; Lane < AIB::MaxRouteLanes; ++Lane)
		{
			if (A.Costs[Lane] != B.Costs[Lane])
			{
				return false;
			}
		}
		return true;
	}

END_DEFINE_SPEC(FAIBRouteBiasSpec)

void FAIBRouteBiasSpec::Define()
{
	It("draws the same costs for the same (match, bot, life) — a seeded run replays", [this]()
	{
		FAIBRouteBias A, B;
		A.Draw(FAIBRouteBias::LifeSeed(42, 3, 1), 0.3f);
		B.Draw(FAIBRouteBias::LifeSeed(42, 3, 1), 0.3f);
		TestTrue(TEXT("byte-identical costs"), SameCosts(A, B));
		TestEqual(TEXT("and the seed they record"), A.Seed, B.Seed);
	});

	It("draws different costs for a different bot, life, or match — no lockstep", [this]()
	{
		FAIBRouteBias Base, OtherBot, OtherLife, OtherMatch;
		Base.Draw(FAIBRouteBias::LifeSeed(42, 3, 1), 0.3f);
		OtherBot.Draw(FAIBRouteBias::LifeSeed(42, 4, 1), 0.3f);
		OtherLife.Draw(FAIBRouteBias::LifeSeed(42, 3, 2), 0.3f);
		OtherMatch.Draw(FAIBRouteBias::LifeSeed(43, 3, 1), 0.3f);
		TestFalse(TEXT("bot 4 is not bot 3"), SameCosts(Base, OtherBot));
		TestFalse(TEXT("life 2 is not life 1"), SameCosts(Base, OtherLife));
		TestFalse(TEXT("match 43 is not match 42"), SameCosts(Base, OtherMatch));
		TestNotEqual(TEXT("the slot changes the seed itself"),
			FAIBRouteBias::LifeSeed(42, 3, 1), FAIBRouteBias::LifeSeed(42, 4, 1));
	});

	It("normalises so the cheapest lane costs exactly 1 and nothing is cheaper", [this]()
	{
		for (int32 Bot = 0; Bot < 16; ++Bot)
		{
			FAIBRouteBias Bias;
			Bias.Draw(FAIBRouteBias::LifeSeed(7, Bot, 1), 0.3f);
			float Min = FLT_MAX, Max = 0.f;
			for (int32 Lane = 1; Lane <= AIB::MaxRouteLanes; ++Lane)
			{
				Min = FMath::Min(Min, Bias.CostOf(Lane));
				Max = FMath::Max(Max, Bias.CostOf(Lane));
			}
			TestEqual(TEXT("cheapest lane is the default cost"), Min, 1.f);
			TestTrue(TEXT("dearest lane is within the spread's ceiling (1.3/0.7)"), Max <= 1.3f / 0.7f + KINDA_SMALL_NUMBER);
		}
	});

	It("treats the spread as the knob — 0 is the conga line, a wild value clamps", [this]()
	{
		FAIBRouteBias Flat;
		Flat.Draw(FAIBRouteBias::LifeSeed(1, 1, 1), 0.f);
		for (int32 Lane = 1; Lane <= AIB::MaxRouteLanes; ++Lane)
		{
			TestEqual(TEXT("spread 0 prices every lane at 1"), Flat.CostOf(Lane), 1.f);
		}
		FAIBRouteBias Wild;
		Wild.Draw(FAIBRouteBias::LifeSeed(1, 1, 1), 5.f);
		for (int32 Lane = 1; Lane <= AIB::MaxRouteLanes; ++Lane)
		{
			TestTrue(TEXT("a clamped spread never zeroes or negates a lane"),
				FMath::IsFinite(Wild.CostOf(Lane)) && Wild.CostOf(Lane) >= 1.f);
		}
	});

	It("prices a non-lane at the default cost, never 0", [this]()
	{
		FAIBRouteBias Bias;
		Bias.Draw(FAIBRouteBias::LifeSeed(9, 0, 1), 0.3f);
		TestEqual(TEXT("lane 0"), Bias.CostOf(0), 1.f);
		TestEqual(TEXT("lane 7"), Bias.CostOf(AIB::MaxRouteLanes + 1), 1.f);
		TestEqual(TEXT("an undrawn bias is flat"), FAIBRouteBias().CostOf(3), 1.f);
	});

	It("round-trips the six lane classes and calls nothing else a lane", [this]()
	{
		for (int32 Lane = 1; Lane <= AIB::MaxRouteLanes; ++Lane)
		{
			const UClass* Class = AIBLanes::ClassOf(Lane);
			TestNotNull(TEXT("a class per lane"), Class);
			TestEqual(TEXT("its ordinal comes back"), AIBLanes::LaneIdOf(Class), Lane);
		}
		TestNull(TEXT("lane 0 has no class"), AIBLanes::ClassOf(0));
		TestNull(TEXT("lane 7 has no class"), AIBLanes::ClassOf(AIB::MaxRouteLanes + 1));
		TestEqual(TEXT("the default area is not a lane"), AIBLanes::LaneIdOf(UNavArea::StaticClass()), 0);
		TestEqual(TEXT("null is not a lane"), AIBLanes::LaneIdOf(nullptr), 0);
	});

	It("ships the row default the csv must mirror", [this]()
	{
		TestEqual(TEXT("RouteLaneWeightSpread"), FAIBTierRow().RouteLaneWeightSpread, 0.3f);
	});

	It("describes itself as six lane:cost pairs — the replay diff's evidence", [this]()
	{
		FAIBRouteBias Bias;
		Bias.Draw(FAIBRouteBias::LifeSeed(42, 3, 1), 0.3f);
		TArray<FString> Pairs;
		Bias.Describe().ParseIntoArray(Pairs, TEXT(","));
		TestEqual(TEXT("six pairs"), Pairs.Num(), AIB::MaxRouteLanes);
		TestTrue(TEXT("first names lane 1"), Pairs[0].StartsWith(TEXT("1:")));
		FAIBRouteBias Again;
		Again.Draw(FAIBRouteBias::LifeSeed(42, 3, 1), 0.3f);
		TestEqual(TEXT("same seed, same text"), Bias.Describe(), Again.Describe());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
