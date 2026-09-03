#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBBotController.h"
#include "Data/AIBDataRows.h"

/**
 * AIB22 H1/H2, proven headless. The stationary-sweep budget is CONTROLLER state so that
 * a StateTree recreating SweepLook's instance data on re-entry cannot refill it (1, 2);
 * it expires exactly at SweepMaxSeconds and 0 means "never stand to sweep" (3); only
 * motion refills it (4). The walking pan stays inside ±Arc, reaches both edges, starts
 * straight ahead and never jumps (5). The row defaults are what the csv must mirror (6).
 */
BEGIN_DEFINE_SPEC(FAIBSweepBudgetSpec, "AIBot.Sim.SweepBudget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** What a task re-entry does to the world: the task's scratch is rebuilt, the
	 *  controller's budget is not. The scratch is a stand-in for the instance data. */
	struct FTaskScratch { float PhaseDegrees = 0.f; bool bLogged = false; };

END_DEFINE_SPEC(FAIBSweepBudgetSpec)

void FAIBSweepBudgetSpec::Define()
{
	It("carries a controller-held budget across a task re-entry — the scratch resets, the spend does not", [this]()
	{
		const float Max = 2.f;
		FAIBSweepBudget Budget;
		{
			FTaskScratch Scratch;
			for (int32 Tick = 0; Tick < 15; ++Tick) { Budget.Spend(0.1f); Scratch.PhaseDegrees += 9.f; }
			TestTrue(TEXT("1.5s in, budget remains"), Budget.HasBudget(Max));
		}
		{
			FTaskScratch Scratch; // re-entry: fresh instance data
			TestEqual(TEXT("scratch reset"), Scratch.PhaseDegrees, 0.f);
			TestTrue(TEXT("budget survived the re-entry"), Budget.SpentSeconds > 1.49f);
			for (int32 Tick = 0; Tick < 4; ++Tick) { Budget.Spend(0.1f); }
			TestTrue(TEXT("1.9s: one tick left"), Budget.HasBudget(Max));
			Budget.Spend(0.1f);
			TestFalse(TEXT("expired at SweepMaxSeconds, not at re-entry + SweepMaxSeconds"), Budget.HasBudget(Max));
		}
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

	It("refills only when reset — the motion sample's job", [this]()
	{
		FAIBSweepBudget Budget;
		Budget.Spend(2.f);
		TestFalse(TEXT("spent"), Budget.HasBudget(2.f));
		Budget.Reset();
		TestTrue(TEXT("a new stop earns a new look"), Budget.HasBudget(2.f));
		TestEqual(TEXT("clean"), Budget.SpentSeconds, 0.f);
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
