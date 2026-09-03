#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBBotController.h"
#include "Data/AIBDataRows.h"

/**
 * AIB22 H1/H2, proven headless. The stationary-sweep budget is CONTROLLER state so that
 * a StateTree recreating SweepLook's instance data on re-entry cannot refill it: the
 * owner outlives the task scratch (1), and a budget COPIED into the scratch does not
 * survive (2 — the negative the W-REVIEW asked for); it expires exactly at
 * SweepMaxSeconds and 0 means "never stand to sweep" (3); motion refills it (4); a NEW
 * post refills it, the same post does not — and "same" is the mover's own at-post band,
 * 1.5x acceptance (5, W-REVIEW #3 M4). The walking pan stays inside ±Arc,
 * reaches both edges, starts straight ahead and never jumps (6). The row defaults are
 * what the csv must mirror (7).
 */
BEGIN_DEFINE_SPEC(FAIBSweepBudgetSpec, "AIBot.Sim.SweepBudget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** The controller: lives for the body's whole life, owns the budget. */
	struct FOwner { FAIBSweepBudget Budget; };
	/** What a completion transition does to a task: its scratch is REBUILT. The right
	 *  scratch reads the owner's budget through a pointer; the wrong one holds a copy. */
	struct FTaskScratch { FAIBSweepBudget* Budget = nullptr; float PhaseDegrees = 0.f; };
	struct FBadTaskScratch { FAIBSweepBudget Budget; float PhaseDegrees = 0.f; };

END_DEFINE_SPEC(FAIBSweepBudgetSpec)

void FAIBSweepBudgetSpec::Define()
{
	It("carries the owner-held budget across a task re-entry — the scratch is rebuilt, the spend is not", [this]()
	{
		const float Max = 2.f;
		FOwner Owner;
		{
			FTaskScratch Scratch{ &Owner.Budget };
			for (int32 Tick = 0; Tick < 15; ++Tick) { Scratch.Budget->Spend(0.1f); Scratch.PhaseDegrees += 9.f; }
			TestTrue(TEXT("1.5s in, budget remains"), Scratch.Budget->HasBudget(Max));
		}
		{
			FTaskScratch Scratch{ &Owner.Budget }; // re-entry: fresh instance data, same owner
			TestEqual(TEXT("scratch reset"), Scratch.PhaseDegrees, 0.f);
			TestTrue(TEXT("budget survived the re-entry"), Scratch.Budget->SpentSeconds > 1.49f);
			for (int32 Tick = 0; Tick < 4; ++Tick) { Scratch.Budget->Spend(0.1f); }
			TestTrue(TEXT("1.9s: one tick left"), Scratch.Budget->HasBudget(Max));
			Scratch.Budget->Spend(0.1f);
			TestFalse(TEXT("expired at SweepMaxSeconds, not at re-entry + SweepMaxSeconds"), Scratch.Budget->HasBudget(Max));
		}
	});

	It("does NOT survive a re-entry when the task holds its own copy — the design the owner exists to forbid", [this]()
	{
		const float Max = 2.f;
		float TotalStood = 0.f;
		for (int32 Entry = 0; Entry < 3; ++Entry)
		{
			FBadTaskScratch Scratch; // every re-entry: a fresh copy, a fresh budget
			TestEqual(TEXT("the copy starts empty on every entry"), Scratch.Budget.SpentSeconds, 0.f);
			while (Scratch.Budget.HasBudget(Max)) { Scratch.Budget.Spend(0.1f); TotalStood += 0.1f; }
		}
		TestTrue(TEXT("three entries stood three budgets — the unbounded stand"), TotalStood > Max * 2.9f);
	});

	It("expires exactly at the ceiling and never refills on its own", [this]()
	{
		FAIBSweepBudget Budget;
		Budget.Spend(1.999f);
		TestTrue(TEXT("just under"), Budget.HasBudget(2.f));
		Budget.Spend(0.001f);
		TestFalse(TEXT("at the ceiling"), Budget.HasBudget(2.f));
		Budget.Spend(-5.f); // an authoring accident, not a refund
		TestFalse(TEXT("negative spend is ignored"), Budget.HasBudget(2.f));
	});

	It("treats a zero ceiling as no stationary sweep at all", [this]()
	{
		FAIBSweepBudget Budget;
		TestFalse(TEXT("0 = never stand to sweep"), Budget.HasBudget(0.f));
	});

	It("refills on the motion reset — the Think sample's job", [this]()
	{
		FAIBSweepBudget Budget;
		Budget.Spend(2.f);
		TestFalse(TEXT("spent"), Budget.HasBudget(2.f));
		Budget.Reset();
		TestTrue(TEXT("a new stop earns a new look"), Budget.HasBudget(2.f));
		TestEqual(TEXT("clean"), Budget.SpentSeconds, 0.f);
	});

	It("refills on a NEW post and not on the same one — LastSweptPost is the mover's key", [this]()
	{
		const float Radius = 150.f;
		FAIBSweepBudget Budget;
		TestTrue(TEXT("the first post is always new"), Budget.ArriveAt(FVector(0, 0, 0), Radius));
		Budget.Spend(2.f);
		TestFalse(TEXT("spent at the post"), Budget.HasBudget(2.f));
		TestFalse(TEXT("re-entering inside acceptance is the same post"), Budget.ArriveAt(FVector(100, 0, 0), Radius));
		TestFalse(TEXT("no refill: the same post stays spent"), Budget.HasBudget(2.f));
		// The at-post band is 1.5R (the mover reads Idle inside it as arrived): a post
		// inside it is the same post, or the 150-225uu band refilled what it stood on.
		TestFalse(TEXT("inside 1.5x acceptance is still the same post"), Budget.ArriveAt(FVector(0, 200, 0), Radius));
		TestFalse(TEXT("no refill inside the band"), Budget.HasBudget(2.f));
		TestFalse(TEXT("exactly 1.5R is the band's edge, same post"), Budget.ArriveAt(FVector(0, 225, 0), Radius));
		TestTrue(TEXT("past 1.5x acceptance is a new post"), Budget.ArriveAt(FVector(0, 300, 0), Radius));
		TestTrue(TEXT("a new post refills"), Budget.HasBudget(2.f));
		TestEqual(TEXT("and becomes the last swept post"), Budget.LastSweptPost, FVector(0, 300, 0));
		Budget.Reset(); // motion, then back to the same post
		TestFalse(TEXT("motion does not forget the post"), Budget.ArriveAt(FVector(0, 300, 0), Radius));
		TestTrue(TEXT("but motion already refilled it"), Budget.HasBudget(2.f));
	});

	It("pans inside ±Arc, reaches both edges, starts ahead and never jumps", [this]()
	{
		const float Arc = 60.f;
		const float Rate = 90.f;
		const float Dt = 1.f / 60.f;
		TestEqual(TEXT("Phase == Arc looks straight ahead"), AIBSweep::PanOffsetDegrees(Arc, Arc), 0.f);
		TestEqual(TEXT("a zero arc is no pan"), AIBSweep::PanOffsetDegrees(123.f, 0.f), 0.f);

		float Phase = Arc;
		float Prev = AIBSweep::PanOffsetDegrees(Phase, Arc);
		float MinSeen = Prev, MaxSeen = Prev, MaxStep = 0.f;
		for (int32 Tick = 0; Tick < 600; ++Tick) // ten seconds: several full cycles
		{
			Phase += Rate * Dt;
			const float Offset = AIBSweep::PanOffsetDegrees(Phase, Arc);
			MinSeen = FMath::Min(MinSeen, Offset);
			MaxSeen = FMath::Max(MaxSeen, Offset);
			MaxStep = FMath::Max(MaxStep, FMath::Abs(Offset - Prev));
			Prev = Offset;
		}
		TestTrue(TEXT("never past +Arc"), MaxSeen <= Arc + KINDA_SMALL_NUMBER);
		TestTrue(TEXT("never past -Arc"), MinSeen >= -Arc - KINDA_SMALL_NUMBER);
		TestTrue(TEXT("reaches the right edge"), MaxSeen > Arc - 2.f);
		TestTrue(TEXT("reaches the left edge"), MinSeen < -Arc + 2.f);
		TestTrue(TEXT("no step exceeds one tick of the sweep rate"), MaxStep <= Rate * Dt + KINDA_SMALL_NUMBER);
	});

	It("defaults the row to the ruled numbers — the csv mirrors these", [this]()
	{
		const FAIBTierRow Row;
		TestEqual(TEXT("SweepMaxSeconds"), Row.SweepMaxSeconds, 2.f);
		TestEqual(TEXT("SweepArcDegrees"), Row.SweepArcDegrees, 60.f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
