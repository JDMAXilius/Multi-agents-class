#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBBotController.h"
#include "Data/AIBDataRows.h"

/**
 * AIB22 5(B), proven headless. The island fact is a CONTROLLER-held latch: exactly
 * IslandLatchDraws consecutive draws with no full path latch it (1), one full draw resets
 * the count (2), a task re-entry that recreates the task's scratch cannot lose the count
 * or the latch (3) and a latch COPIED into the scratch does not survive (4 — the negative
 * the W-REVIEW asked for), Egress's gate is false until the latch and after every clear
 * (5), the latching draw reports itself exactly once (6), an Egress-failure cooldown
 * blocks latching (7), a latch past LatchMaxAgeSeconds reads unlatched and clears (8), a
 * completed full-path move clears (9), the row defaults are what the csv must mirror
 * (10), the gate's confirmation is cached once per latch and forgotten on every clear
 * and re-latch (11, W-REVIEW #3 M6), a lipless egress on a confirmed island STRANDS
 * for the cooldown — dropped by the cooldown or a completed full-path move (12, #3 H2),
 * and the confirmation is a decision over an ANCHOR LIST: island iff NO anchor has a full
 * path, ONE full path refutes (clearing with the cooldown) and names itself, an empty
 * list confirms nothing (13, fix #4 R1).
 */
BEGIN_DEFINE_SPEC(FAIBIslandLatchSpec, "AIBot.Sim.IslandLatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** The controller: lives for the body's whole life, owns the latch. */
	struct FOwner { FAIBIslandLatch Latch; };
	/** A stand-in for Wander's instance data: rebuilt on every re-entry, unlike the latch.
	 *  The right scratch reads the owner's latch through a pointer; the wrong one holds a copy. */
	struct FTaskScratch { FAIBIslandLatch* Latch = nullptr; int32 DrawsThisEntry = 0; };
	struct FBadTaskScratch { FAIBIslandLatch Latch; int32 DrawsThisEntry = 0; };

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
		FOwner Owner; // the controller's
		{
			FTaskScratch Scratch{ &Owner.Latch }; // first EnterState: one bad draw, then the want moves on
			Scratch.Latch->NoteDraw(false, Draws, 5.0);
			++Scratch.DrawsThisEntry;
			TestEqual(TEXT("one draw this entry"), Scratch.DrawsThisEntry, 1);
		}
		{
			FTaskScratch Scratch{ &Owner.Latch }; // re-entry: fresh instance data, same owner
			TestEqual(TEXT("scratch reset"), Scratch.DrawsThisEntry, 0);
			TestEqual(TEXT("the count survived the re-entry"), Scratch.Latch->BadDraws, 1);
			Scratch.Latch->NoteDraw(false, Draws, 6.0);
			TestTrue(TEXT("two more latch — three across two entries"), Scratch.Latch->NoteDraw(false, Draws, 6.1));
		}
		{
			FTaskScratch Scratch{ &Owner.Latch }; // Egress's own re-entry mid-tactic
			TestTrue(TEXT("the latch survived too"), Scratch.Latch->bOnIsland);
			TestEqual(TEXT("with its stranded clock"), Scratch.Latch->LatchedAtSeconds, 6.1);
		}
	});

	It("does NOT survive a re-entry when the task holds its own copy — the design the owner exists to forbid", [this]()
	{
		int32 Latches = 0;
		for (int32 Entry = 0; Entry < 6; ++Entry)
		{
			FBadTaskScratch Scratch; // every re-entry: a fresh copy, a fresh count
			TestEqual(TEXT("the copy starts empty on every entry"), Scratch.Latch.BadDraws, 0);
			Latches += Scratch.Latch.NoteDraw(false, Draws, Entry * 1.0) ? 1 : 0; // one draw per entry
			TestFalse(TEXT("one draw per entry never reaches three"), Scratch.Latch.bOnIsland);
		}
		TestEqual(TEXT("six bad draws across six entries, no latch — the fact never forms"), Latches, 0);
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

	It("does not latch during the Egress cooldown, and measures again after it", [this]()
	{
		FAIBIslandLatch Latch;
		Latch.NoteDraw(false, Draws, 0.0);
		Latch.NoteDraw(false, Draws, 0.1);
		Latch.NoteDraw(false, Draws, 0.2);
		TestTrue(TEXT("latched"), Latch.bOnIsland);
		Latch.ClearWithCooldown(1.0, 5.f); // Egress failed: no lip
		TestFalse(TEXT("cleared"), Latch.bOnIsland);
		for (int32 Draw = 0; Draw < 10; ++Draw)
		{
			TestFalse(TEXT("a draw inside the cooldown never latches"), Latch.NoteDraw(false, Draws, 1.0 + Draw * 0.4));
		}
		TestFalse(TEXT("still clear at 4.6s"), Latch.bOnIsland);
		TestEqual(TEXT("and nothing counted toward the next latch"), Latch.BadDraws, 0);
		Latch.NoteDraw(false, Draws, 6.0);
		Latch.NoteDraw(false, Draws, 6.1);
		TestTrue(TEXT("three draws after the cooldown latch again"), Latch.NoteDraw(false, Draws, 6.2));
		Latch.Reset();
		TestEqual(TEXT("possession drops the cooldown stamp too"), Latch.NoLatchBeforeSeconds, -1.0);
	});

	It("reads a latch older than LatchMaxAgeSeconds as unlatched, and clears it", [this]()
	{
		FAIBIslandLatch Latch;
		Latch.NoteDraw(false, Draws, 0.0);
		Latch.NoteDraw(false, Draws, 0.0);
		Latch.NoteDraw(false, Draws, 0.0);
		TestTrue(TEXT("fresh: latched"), Latch.ReadLatched(9.9, 10.f));
		TestTrue(TEXT("ageless (0) never expires"), Latch.ReadLatched(1000.0, 0.f));
		TestFalse(TEXT("past the age: reads unlatched"), Latch.ReadLatched(10.1, 10.f));
		TestFalse(TEXT("and it cleared"), Latch.bOnIsland);
		TestEqual(TEXT("count with it"), Latch.BadDraws, 0);
		TestEqual(TEXT("clock with it"), Latch.LatchedAtSeconds, -1.0);
	});

	It("clears on a completed full-path move — any state, no cooldown", [this]()
	{
		FAIBIslandLatch Latch;
		Latch.NoteDraw(false, Draws, 0.0);
		Latch.NoteDraw(false, Draws, 0.0);
		Latch.NoteDraw(false, Draws, 0.0);
		TestTrue(TEXT("latched"), Latch.bOnIsland);
		Latch.Clear(); // OnMoveCompleted: DidMoveReachGoal
		TestFalse(TEXT("cleared"), Latch.bOnIsland);
		TestEqual(TEXT("no cooldown from a clear"), Latch.NoLatchBeforeSeconds, -1.0);
		Latch.NoteDraw(false, Draws, 0.5);
		Latch.NoteDraw(false, Draws, 0.6);
		TestTrue(TEXT("the next three draws may latch at once"), Latch.NoteDraw(false, Draws, 0.7));
	});

	It("caches the confirmation once per latch, and forgets it on every clear and re-latch", [this]()
	{
		using EConfirm = FAIBIslandLatch::EConfirm;
		FAIBIslandLatch Latch;
		TestTrue(TEXT("fresh: untested"), Latch.Confirmation == EConfirm::Untested);
		Latch.NoteDraw(false, Draws, 0.0);
		Latch.NoteDraw(false, Draws, 0.1);
		Latch.NoteDraw(false, Draws, 0.2);
		TestTrue(TEXT("a new latch is untested — the gate runs its ONE test"), Latch.Confirmation == EConfirm::Untested);
		Latch.Confirmation = EConfirm::Island; // the gate's one TestPathSync said island
		TestTrue(TEXT("held while latched: every later evaluation reads it"), Latch.ReadLatched(1.0, 10.f) && Latch.Confirmation == EConfirm::Island);
		Latch.Clear(); // a completed full-path move
		TestTrue(TEXT("a clear forgets it"), Latch.Confirmation == EConfirm::Untested);
		Latch.NoteDraw(false, Draws, 2.0);
		Latch.NoteDraw(false, Draws, 2.1);
		Latch.NoteDraw(false, Draws, 2.2);
		Latch.ClearWithCooldown(2.3, 5.f); // the gate refuted it
		Latch.Confirmation = EConfirm::Refuted;
		TestFalse(TEXT("refuted is not latched"), Latch.ReadLatched(2.4, 10.f));
		Latch.NoteDraw(false, Draws, 8.0);
		Latch.NoteDraw(false, Draws, 8.1);
		TestTrue(TEXT("re-latched after the cooldown"), Latch.NoteDraw(false, Draws, 8.2));
		TestTrue(TEXT("a re-latch is untested again — never a stale verdict"), Latch.Confirmation == EConfirm::Untested);
	});

	It("strands after a lipless egress on a confirmed island — no latch, no draw — until the cooldown or a full-path move", [this]()
	{
		FAIBIslandLatch Latch;
		TestFalse(TEXT("fresh: not stranded"), Latch.IsStranded(0.0));
		Latch.NoteDraw(false, Draws, 0.0);
		Latch.NoteDraw(false, Draws, 0.1);
		Latch.NoteDraw(false, Draws, 0.2);
		Latch.Confirmation = FAIBIslandLatch::EConfirm::Island;
		Latch.Strand(1.0, 5.f); // Egress: no legal lip
		TestFalse(TEXT("the latch is cleared"), Latch.bOnIsland);
		TestTrue(TEXT("stranded at once"), Latch.IsStranded(1.0));
		TestTrue(TEXT("stranded through the cooldown"), Latch.IsStranded(5.9));
		TestFalse(TEXT("a draw inside the cooldown never latches"), Latch.NoteDraw(false, Draws, 3.0));
		TestFalse(TEXT("the cooldown lapsing ends it"), Latch.IsStranded(6.0));
		TestFalse(TEXT("and it stays ended"), Latch.IsStranded(7.0));

		Latch.NoteDraw(false, Draws, 8.0);
		Latch.NoteDraw(false, Draws, 8.1);
		Latch.NoteDraw(false, Draws, 8.2);
		Latch.Strand(9.0, 5.f);
		TestTrue(TEXT("stranded again on the next latch"), Latch.IsStranded(9.5));
		Latch.Clear(); // OnMoveCompleted: DidMoveReachGoal — connected ground after all
		TestFalse(TEXT("a completed full-path move ends it early"), Latch.IsStranded(9.6));
		TestTrue(TEXT("the cooldown stamp itself is untouched by the clear"), Latch.NoLatchBeforeSeconds == 14.0);

		Latch.Strand(20.0, 5.f);
		Latch.Reset(); // possession
		TestFalse(TEXT("a new life is not stranded"), Latch.IsStranded(20.1));
	});

	It("confirms against the anchor LIST — island iff every anchor fails, one full path refutes, none confirms nothing", [this]()
	{
		using EConfirm = FAIBIslandLatch::EConfirm;
		auto Latched = []()
		{
			FAIBIslandLatch Latch;
			Latch.NoteDraw(false, Draws, 0.0);
			Latch.NoteDraw(false, Draws, 0.1);
			Latch.NoteDraw(false, Draws, 0.2);
			return Latch;
		};

		// The corner spawn pad: the objective and every PlayerStart all fail (fix #6 F6-1:
		// the last full-path move is no anchor — Egress's own lip walk was that move).
		{
			FAIBIslandLatch Latch = Latched();
			const bool NoneReach[] = { false, false, false, false };
			TestEqual(TEXT("no refuter"), Latch.Confirm(NoneReach, 1.0, 5.f), static_cast<int32>(INDEX_NONE));
			TestTrue(TEXT("island"), Latch.Confirmation == EConfirm::Island);
			TestTrue(TEXT("still latched"), Latch.ReadLatched(1.1, 10.f));
			TestEqual(TEXT("no cooldown from a confirmation"), Latch.NoLatchBeforeSeconds, -1.0);
		}
		// The floor bot: its own spawn pad is an island but the objective reaches — refuted.
		{
			FAIBIslandLatch Latch = Latched();
			const bool ObjectiveReaches[] = { true, false, false };
			TestEqual(TEXT("the first anchor refutes"), Latch.Confirm(ObjectiveReaches, 1.0, 5.f), 0);
			TestTrue(TEXT("refuted"), Latch.Confirmation == EConfirm::Refuted);
			TestFalse(TEXT("cleared"), Latch.bOnIsland);
			TestEqual(TEXT("with the cooldown"), Latch.NoLatchBeforeSeconds, 6.0);
			TestFalse(TEXT("a draw inside the cooldown does not re-latch"), Latch.NoteDraw(false, Draws, 2.0));
		}
		// One success anywhere in the list is enough, and it names the anchor.
		{
			FAIBIslandLatch Latch = Latched();
			const bool LastPlayerStartReaches[] = { false, false, false, true };
			TestEqual(TEXT("the fourth anchor refutes"), Latch.Confirm(LastPlayerStartReaches, 1.0, 5.f), 3);
			TestTrue(TEXT("refuted"), Latch.Confirmation == EConfirm::Refuted);
		}
		// Nothing to test against (no anchor on the mesh): not a confirmation, not a refutation.
		{
			FAIBIslandLatch Latch = Latched();
			TestEqual(TEXT("no refuter"), Latch.Confirm(TConstArrayView<bool>(), 1.0, 5.f), static_cast<int32>(INDEX_NONE));
			TestTrue(TEXT("untested"), Latch.Confirmation == EConfirm::Untested);
			TestTrue(TEXT("the latch holds — the gate acts on it alone"), Latch.bOnIsland);
		}
		// A spawn pad alone, failing, would have confirmed the OLD single-anchor gate on
		// open ground as readily as on an island — the list is what makes a fail mean something.
		{
			FAIBIslandLatch Latch = Latched();
			const bool SpawnOnly[] = { false };
			Latch.Confirm(SpawnOnly, 1.0, 5.f);
			TestTrue(TEXT("one failing anchor still confirms — the LIST must carry the objective and the PlayerStarts"), Latch.Confirmation == EConfirm::Island);
		}
	});

	It("defaults the row to the ruled numbers — the csv mirrors these", [this]()
	{
		const FAIBTierRow Row;
		TestEqual(TEXT("IslandLatchDraws"), Row.IslandLatchDraws, 3);
		TestEqual(TEXT("IslandLipStandoffUU"), Row.IslandLipStandoffUU, 60.f);
		TestEqual(TEXT("IslandLipProbeUU"), Row.IslandLipProbeUU, 150.f);
		TestEqual(TEXT("IslandMinDropUU"), Row.IslandMinDropUU, 120.f);
		TestEqual(TEXT("EgressCooldownSeconds"), Row.EgressCooldownSeconds, 5.f);
		TestEqual(TEXT("LatchMaxAgeSeconds"), Row.LatchMaxAgeSeconds, 10.f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
