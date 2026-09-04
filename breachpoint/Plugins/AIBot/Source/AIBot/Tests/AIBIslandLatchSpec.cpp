#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBBotController.h"
#include "Core/AIBTypes.h"
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
 *
 * (14, HIGH-1, 3 Sep) THE FAILURE SEQUENCE, not the ring: the last block drives the two
 * calls the CALL SITE makes in the order it makes them — blame the door, then strand or
 * clear-with-cooldown — because every test above passed while the blacklist was inert in
 * the shipped build. Strand and Clear were sharing one body and wanted opposite memory:
 * "still up here and that door failed" keeps the grudge, "off the island" alone forgets.
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

	It("F7-3: a landing inside the fan's footprint at the step-off height is the same island", [this]()
	{
		// The gantry top as the fan measures it: x 1000–1400, y 1800–2200, nav and feet at z 810.
		FBox Footprint(ForceInit);
		Footprint += FVector(1000.0, 1800.0, 810.0);
		Footprint += FVector(1400.0, 2200.0, 810.0);
		TestTrue(TEXT("inside, same height"), AIB::LandedOnSameIsland(Footprint, FVector(1289.0, 1950.0, 812.0)));
		TestTrue(TEXT("40uu past the edge is inside the ±50 slack"), AIB::LandedOnSameIsland(Footprint, FVector(1440.0, 1950.0, 810.0)));
		TestTrue(TEXT("29uu higher is the same height"), AIB::LandedOnSameIsland(Footprint, FVector(1200.0, 2000.0, 839.0)));
		TestFalse(TEXT("60uu past the edge left it"), AIB::LandedOnSameIsland(Footprint, FVector(1460.0, 1950.0, 810.0)));
		TestFalse(TEXT("453uu below is the floor, whatever the XY"), AIB::LandedOnSameIsland(Footprint, FVector(1289.0, 1950.0, 357.0)));
		TestFalse(TEXT("31uu higher is another deck"), AIB::LandedOnSameIsland(Footprint, FVector(1200.0, 2000.0, 841.0)));
		TestFalse(TEXT("an unmeasured footprint says nothing"), AIB::LandedOnSameIsland(FBox(ForceInit), FVector(1200.0, 2000.0, 810.0)));
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

	Describe("the interior-lip blacklist (AIB22 follow-up (c))", [this]()
	{
		// The gantry residual. The lip fan is DETERMINISTIC from the same feet BY DESIGN
		// ("the same feet must find the same lip"), so a lip that failed is handed back on
		// the next entry, and the next, until the match ends. A body that cannot learn
		// which door does not open keeps trying that door.

		It("refuses a failed lip, and only inside the window", [this]()
		{
			FAIBIslandLatch Latch;
			const FVector Door(1000.f, 0.f, 400.f);
			TestFalse(TEXT("nothing refused yet"), Latch.RefusesLip(Door, 1.0, 120.f));
			Latch.NoteLipFailed(Door, 1.0, 20.f);
			TestTrue(TEXT("refused on the next entry"), Latch.RefusesLip(Door, 1.5, 120.f));
			TestTrue(TEXT("still refused inside the window"), Latch.RefusesLip(Door, 20.0, 120.f));
			TestFalse(TEXT("the window lapses"), Latch.RefusesLip(Door, 21.5, 120.f));
		});

		It("refuses by RADIUS, because the fan re-derives a lip from shifted feet", [this]()
		{
			FAIBIslandLatch Latch;
			Latch.NoteLipFailed(FVector(1000.f, 0.f, 400.f), 1.0, 20.f);
			TestTrue(TEXT("60uu away is the same door"),
				Latch.RefusesLip(FVector(1060.f, 0.f, 400.f), 2.0, 120.f));
			TestFalse(TEXT("300uu away is a different door"),
				Latch.RefusesLip(FVector(1300.f, 0.f, 400.f), 2.0, 120.f));
		});

		It("holds only a few doors — the oldest is evicted, never an unbounded list", [this]()
		{
			// An unbounded blacklist is a leak that ALSO eventually refuses every lip on
			// the map, which would strand the bot by the very mechanism meant to free it.
			FAIBIslandLatch Latch;
			for (int32 i = 0; i < FAIBIslandLatch::MaxFailedLips + 1; ++i)
			{
				Latch.NoteLipFailed(FVector(1000.f + i * 500.f, 0.f, 400.f), 1.0, 20.f);
			}
			TestFalse(TEXT("the oldest was evicted"),
				Latch.RefusesLip(FVector(1000.f, 0.f, 400.f), 2.0, 120.f));
			TestTrue(TEXT("the newest is held"),
				Latch.RefusesLip(FVector(1000.f + FAIBIslandLatch::MaxFailedLips * 500.f, 0.f, 400.f),
					2.0, 120.f));
		});

		It("only blames a door it was actually trying", [this]()
		{
			// A HORIZONTAL walk to the mesh has no lip at all; blacklisting its target
			// would refuse a legitimate lip that happened to sit near it.
			FAIBIslandLatch Latch;
			Latch.NotePendingLipFailed(1.0, 20.f);
			TestFalse(TEXT("nothing pending, nothing recorded"),
				Latch.RefusesLip(FVector::ZeroVector, 2.0, 120.f));

			Latch.NotePendingLip(FVector(1000.f, 0.f, 400.f));
			Latch.NotePendingLipFailed(1.0, 20.f);
			TestTrue(TEXT("the door it was trying is refused"),
				Latch.RefusesLip(FVector(1000.f, 0.f, 400.f), 2.0, 120.f));

			// Consumed ONCE: a later report with nothing pending must not re-blame the
			// same door and silently extend its sentence past the window.
			Latch.NotePendingLipFailed(50.0, 20.f);
			TestFalse(TEXT("the sentence was not extended"),
				Latch.RefusesLip(FVector(1000.f, 0.f, 400.f), 25.0, 120.f));
		});

		It("drops the pending record when the body actually leaves", [this]()
		{
			// Airborne off the edge = the lip worked. Whatever the landing turns out to
			// be, this door is not to blame — or the blacklist fills with lips that work.
			FAIBIslandLatch Latch;
			Latch.NotePendingLip(FVector(1000.f, 0.f, 400.f));
			Latch.bHasPendingLip = false;                 // what the airborne tick does
			Latch.NotePendingLipFailed(2.0, 20.f);        // a later, unrelated failure
			TestFalse(TEXT("a lip that worked is never blacklisted"),
				Latch.RefusesLip(FVector(1000.f, 0.f, 400.f), 3.0, 120.f));
		});

		It("forgets every grudge the moment the bot is off the island", [this]()
		{
			FAIBIslandLatch Latch;
			Latch.NoteLipFailed(FVector(1000.f, 0.f, 400.f), 1.0, 20.f);
			Latch.Clear();
			TestFalse(TEXT("off the island: no grudge"),
				Latch.RefusesLip(FVector(1000.f, 0.f, 400.f), 2.0, 120.f));
		});
	});

	Describe("the blacklist's failure SEQUENCE, not its data structure (HIGH-1)", [this]()
	{
		// WHY THIS BLOCK EXISTS. Everything above drives NoteLipFailed / RefusesLip
		// DIRECTLY and proves the ring works — and all of it passed while the blacklist
		// was inert in the shipped build, because no test ever ran the two calls the CALL
		// SITE makes, in the order it makes them. A failure recorded a door and then
		// stranded, and Strand cleared through Clear(), which forgets every door: the
		// entry died one statement after it was born, every time, so `lip refused` could
		// not appear in any log this build produced. The structure was never the bug. The
		// SEQUENCE was, and a spec that steps over the seam is not evidence.

		It("keeps the door when the very failure that recorded it STRANDS the body", [this]()
		{
			// THE REGRESSION. This is TickOffMeshRecovery's give-up, line for line:
			// blame the door, then strand for the cooldown. Fails before the fix.
			FAIBIslandLatch Latch;
			const FVector Door(1289.f, 1950.f, 810.f);   // the gantry lip, from the watch
			Latch.NotePendingLip(Door);
			Latch.NotePendingLipFailed(10.0, 20.f);
			Latch.Strand(10.0, 5.f);
			TestTrue(TEXT("stranded is STILL UP HERE: the door stays shut"),
				Latch.RefusesLip(Door, 10.0, 120.f));
			TestTrue(TEXT("and on the next entry, after the cooldown"),
				Latch.RefusesLip(Door, 16.0, 120.f));
			TestTrue(TEXT("the stranding itself still works"), Latch.IsStranded(11.0));
			TestFalse(TEXT("and the window still lapses on its own"),
				Latch.RefusesLip(Door, 31.0, 120.f));
		});

		It("keeps the door through an Egress failure's cooldown clear", [this]()
		{
			// BeginEgress's no-lip exit, the same-island landing, the lip that never
			// dropped: all of them ClearWithCooldown while the body is still up there.
			FAIBIslandLatch Latch;
			const FVector Door(1289.f, 1950.f, 810.f);
			Latch.NotePendingLip(Door);
			Latch.NotePendingLipFailed(10.0, 20.f);
			Latch.ClearWithCooldown(10.0, 5.f);
			TestTrue(TEXT("the door stays shut"), Latch.RefusesLip(Door, 12.0, 120.f));
			TestFalse(TEXT("the latch itself is gone"), Latch.bOnIsland);
			for (int32 i = 0; i < Draws; ++i)
			{
				TestFalse(TEXT("and the cooldown still blocks latching"), Latch.NoteDraw(false, Draws, 12.0));
			}
		});

		It("does not treat a STALE latch as proof the body left", [this]()
		{
			// ReadLatched ages the HYPOTHESIS out. Where the body is standing is not a
			// thing the clock knows, so the doors it proved shut are not the clock's to
			// forgive — forgiving them here re-opens the loop with extra steps.
			FAIBIslandLatch Latch;
			const FVector Door(1289.f, 1950.f, 810.f);
			for (int32 i = 0; i < Draws; ++i) { Latch.NoteDraw(false, Draws, 1.0); }
			Latch.NoteLipFailed(Door, 1.0, 20.f);
			TestTrue(TEXT("latched"), Latch.ReadLatched(2.0, 10.f));
			TestFalse(TEXT("stale, so unlatched"), Latch.ReadLatched(15.0, 10.f));
			TestTrue(TEXT("but the door is still shut"), Latch.RefusesLip(Door, 15.0, 120.f));
		});

		It("drops an UNJUDGED door rather than letting a later failure blame it", [this]()
		{
			// The lip walk that never reached the lip, and Egress's ExitState: nobody
			// judged this door, so nobody may convict it — and it must not be lying
			// around for the next failure, in another tactic, to convict by accident.
			FAIBIslandLatch Latch;
			const FVector Chosen(1289.f, 1950.f, 810.f);
			const FVector Other(2500.f, 1950.f, 810.f);
			Latch.NotePendingLip(Chosen);
			Latch.ClearWithCooldown(10.0, 5.f);       // the walk failed, not the door
			TestFalse(TEXT("nothing pending after the clear"), Latch.bHasPendingLip);
			Latch.NotePendingLip(Other);
			Latch.NotePendingLipFailed(20.0, 20.f);   // a later, unrelated door fails
			TestFalse(TEXT("the untried door was never blamed"),
				Latch.RefusesLip(Chosen, 21.0, 120.f));
			TestTrue(TEXT("the door that did fail is blamed"),
				Latch.RefusesLip(Other, 21.0, 120.f));
		});

		It("forgets only on the clear that means OFF THE ISLAND", [this]()
		{
			FAIBIslandLatch Latch;
			const FVector Door(1289.f, 1950.f, 810.f);
			Latch.NoteLipFailed(Door, 1.0, 20.f);
			Latch.Clear();                            // a completed full-path move, a landing
			TestFalse(TEXT("Clear forgets"), Latch.RefusesLip(Door, 2.0, 120.f));

			Latch.NoteLipFailed(Door, 1.0, 20.f);
			Latch.Reset();                            // possession: a new body, a new map
			TestFalse(TEXT("Reset forgets"), Latch.RefusesLip(Door, 2.0, 120.f));
		});

		It("hands the fan a DIFFERENT door on the next entry, and gives up rather than repeat", [this]()
		{
			// ARM (b) AS A DECISION. The lip fan is deterministic from the same feet BY
			// DESIGN, so the candidate list below is literally what the second entry sees
			// again: nearest first, skip what the latch refuses, take the first survivor.
			// FindIslandLip's own version of this loop is world-bound (navmesh rays,
			// landing path tests) and only PIE can run it — what is provable headless is
			// that the CHOICE changes once a door is blacklisted, which is the whole
			// behaviour the gantry watch found missing.
			const FVector Doors[] = {                 // nearest first, as the fan sorts them
				FVector(1289.f, 1950.f, 810.f),
				FVector(1650.f, 1950.f, 810.f),
			};
			FAIBIslandLatch Latch;
			auto Fan = [&Doors, &Latch](double Now, FVector& Out) -> bool
			{
				for (const FVector& Door : Doors)
				{
					if (!Latch.RefusesLip(Door, Now, 120.f)) { Out = Door; return true; }
				}
				return false;
			};

			FVector First = FVector::ZeroVector;
			TestTrue(TEXT("first entry finds a lip"), Fan(1.0, First));
			TestTrue(TEXT("the nearest one"), First.Equals(Doors[0], 1.f));

			// It failed: the step-off landed on the same island (F7-3).
			Latch.NotePendingLip(First);
			Latch.NotePendingLipFailed(5.0, 20.f);
			Latch.ClearWithCooldown(5.0, 5.f);

			FVector Second = FVector::ZeroVector;
			TestTrue(TEXT("second entry still finds a lip"), Fan(11.0, Second));
			TestFalse(TEXT("and it is NOT the door that just failed"), Second.Equals(First, 1.f));

			// The other one fails too: every door in reach is shut, and the honest answer
			// is no lip — stand for the window — not the first door for the tenth time.
			Latch.NotePendingLip(Second);
			Latch.NotePendingLipFailed(15.0, 20.f);
			Latch.Strand(15.0, 5.f);
			FVector Third = FVector::ZeroVector;
			TestFalse(TEXT("no lip while both are shut"), Fan(20.0, Third));
			TestTrue(TEXT("and the doors re-open when the windows lapse"), Fan(36.0, Third));
		});
	});

}

#endif // WITH_DEV_AUTOMATION_TESTS
