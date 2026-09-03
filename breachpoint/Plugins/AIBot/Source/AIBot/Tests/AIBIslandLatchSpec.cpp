#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBBotController.h"
#include "Data/AIBDataRows.h"

/**
 * AIB22 5(B), proven headless. The island fact is a CONTROLLER-held latch: exactly
 * IslandLatchDraws consecutive draws with no full path latch it (1), one full draw resets
 * the count (2), a task re-entry that recreates the task's scratch cannot lose the count
 * or the latch (3), Egress's gate — the bare bOnIsland read — is false until the latch
 * and after every clear (4), the latching draw reports itself exactly once (5), and the
 * row defaults are what the csv must mirror (6).
 */
BEGIN_DEFINE_SPEC(FAIBIslandLatchSpec, "AIBot.Sim.IslandLatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** A stand-in for Wander's instance data: rebuilt on every re-entry, unlike the latch. */
	struct FTaskScratch { int32 DrawsThisEntry = 0; bool bHasGoal = false; };

	static constexpr int32 Draws = 3;

END_DEFINE_SPEC(FAIBIslandLatchSpec)

void FAIBIslandLatchSpec::Define()
{
	It("latches after exactly three consecutive bad draws, not two", [this]()
	{
		FAIBIslandLatch Latch;
		TestFalse(TEXT("first bad draw"), Latch.NoteDraw(false, Draws, 10.0));
		TestFalse(TEXT("still unlatched"), Latch.bOnIsland);
		TestFalse(TEXT("second bad draw"), Latch.NoteDraw(false, Draws, 10.1));
		TestFalse(TEXT("two is not three"), Latch.bOnIsland);
		TestTrue(TEXT("third bad draw latches"), Latch.NoteDraw(false, Draws, 10.2));
		TestTrue(TEXT("latched"), Latch.bOnIsland);
		TestEqual(TEXT("stranded clock stamped at the latch"), Latch.LatchedAtSeconds, 10.2);
	});

	It("resets the count on a full draw — consecutive means consecutive", [this]()
	{
		FAIBIslandLatch Latch;
		Latch.NoteDraw(false, Draws, 1.0);
		Latch.NoteDraw(false, Draws, 1.1);
		Latch.NoteDraw(true, Draws, 1.2);
		TestEqual(TEXT("a full draw zeroes the count"), Latch.BadDraws, 0);
		Latch.NoteDraw(false, Draws, 1.3);
		Latch.NoteDraw(false, Draws, 1.4);
		TestFalse(TEXT("two after the reset is not three"), Latch.bOnIsland);
		TestTrue(TEXT("the third consecutive one is"), Latch.NoteDraw(false, Draws, 1.5));
		Latch.NoteDraw(true, Draws, 1.6);
		TestFalse(TEXT("a full draw clears a held latch"), Latch.bOnIsland);
		TestEqual(TEXT("and its clock"), Latch.LatchedAtSeconds, -1.0);
	});

	It("survives a task re-entry — the scratch is recreated, the controller's latch is not", [this]()
	{
		FAIBIslandLatch Latch; // the controller's
		{
			FTaskScratch Scratch; // first EnterState: one bad draw, then the want moves on
			Latch.NoteDraw(false, Draws, 5.0);
			++Scratch.DrawsThisEntry;
			TestEqual(TEXT("one draw this entry"), Scratch.DrawsThisEntry, 1);
		}
		{
			FTaskScratch Scratch; // re-entry: fresh instance data
			TestEqual(TEXT("scratch reset"), Scratch.DrawsThisEntry, 0);
			TestEqual(TEXT("the count survived the re-entry"), Latch.BadDraws, 1);
			Latch.NoteDraw(false, Draws, 6.0);
			TestTrue(TEXT("two more latch — three across two entries"), Latch.NoteDraw(false, Draws, 6.1));
		}
		{
			FTaskScratch Scratch; // Egress's own re-entry mid-tactic
			TestTrue(TEXT("the latch survived too"), Latch.bOnIsland);
			TestEqual(TEXT("with its stranded clock"), Latch.LatchedAtSeconds, 6.1);
		}
	});

	It("never gates Egress in while unlatched, and lets go on every clear", [this]()
	{
		FAIBIslandLatch Latch;
		TestFalse(TEXT("fresh controller"), Latch.bOnIsland);
		Latch.NoteDraw(false, Draws, 0.0);
		Latch.NoteDraw(false, Draws, 0.1);
		TestFalse(TEXT("two bad draws"), Latch.bOnIsland);
		Latch.NoteDraw(false, Draws, 0.2);
		TestTrue(TEXT("three"), Latch.bOnIsland);
		Latch.Reset(); // the landing, or a failed egress
		TestFalse(TEXT("cleared by the landing"), Latch.bOnIsland);
		TestEqual(TEXT("count cleared with it"), Latch.BadDraws, 0);
		TestEqual(TEXT("clock cleared with it"), Latch.LatchedAtSeconds, -1.0);
	});

	It("reports the latching draw once — later bad draws stay silent, a zero threshold means one", [this]()
	{
		FAIBIslandLatch Latch;
		Latch.NoteDraw(false, Draws, 0.0);
		Latch.NoteDraw(false, Draws, 0.0);
		TestTrue(TEXT("third reports"), Latch.NoteDraw(false, Draws, 0.0));
		TestFalse(TEXT("fourth does not"), Latch.NoteDraw(false, Draws, 0.0));
		TestEqual(TEXT("but still counts"), Latch.BadDraws, 4);
		FAIBIslandLatch Degenerate;
		TestTrue(TEXT("an authored 0 clamps to 1, never to never"), Degenerate.NoteDraw(false, 0, 0.0));
	});

	It("defaults the row to the ruled numbers — the csv mirrors these", [this]()
	{
		const FAIBTierRow Row;
		TestEqual(TEXT("IslandLatchDraws"), Row.IslandLatchDraws, 3);
		TestEqual(TEXT("IslandLipStandoffUU"), Row.IslandLipStandoffUU, 60.f);
		TestEqual(TEXT("IslandLipProbeUU"), Row.IslandLipProbeUU, 150.f);
		TestEqual(TEXT("IslandMinDropUU"), Row.IslandMinDropUU, 120.f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
