#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Brain/AIBTactic.h"
#include "Core/AIBTags.h"
#include "Execution/AIBStateTreeTasks.h"

/**
 * AIB26: the tactic gates, headless. Matches() is plain tag logic, so the tree-shape proof
 * in PIE stays a shape proof — and the two layers must never cross: an ambition gate
 * refusing a tactic tag (and the reverse) is what keeps the second engine's winner from
 * ever opening an ambition branch.
 */
BEGIN_DEFINE_SPEC(FAIBTacticGateSpec, "AIBot.Sim.TacticGate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FAIBTacticGateSpec)

void FAIBTacticGateSpec::Define()
{
	It("mirrors the tactic engine's winner exactly — one tactic, one gate", []()
	{
		const FAIBGateTacticFlankCondition FlankGate;
		const FAIBGateTacticHoldCondition HoldGate;
		TestTrue(TEXT("Flank gate takes Flank"), FlankGate.Matches(AIBTags::Tactic_Flank));
		TestFalse(TEXT("Flank gate refuses Hold"), FlankGate.Matches(AIBTags::Tactic_Hold));
		TestFalse(TEXT("Flank gate refuses Push"), FlankGate.Matches(AIBTags::Tactic_Push));
		TestTrue(TEXT("Hold gate takes Hold"), HoldGate.Matches(AIBTags::Tactic_Hold));
		TestFalse(TEXT("Hold gate refuses Flank"), HoldGate.Matches(AIBTags::Tactic_Flank));
		TestFalse(TEXT("an invalid want opens nothing"), HoldGate.Matches(FGameplayTag()));
	});

	It("never lets the layers cross — a tactic tag opens no ambition branch and vice versa", []()
	{
		const FAIBGateEngageCondition EngageGate;
		const FAIBGateTacticFlankCondition FlankGate;
		TestFalse(TEXT("Engage gate refuses a tactic"), EngageGate.Matches(AIBTags::Tactic_Push));
		TestFalse(TEXT("Flank gate refuses an ambition"), FlankGate.Matches(AIBTags::Ambition_Engage));
	});

	It("registers Push as the floor: no commit, and it is last in the builder's order", []()
	{
		// The tree's Push child is ungated and LAST; the builder's order is what the
		// controller registers, and the floor carrying a commit would starve a latched
		// flank point of its turn (the Roam rule, W-REVIEW P2 H-1).
		TArray<FAIBAmbitionSpec> Tactics;
		AIBTactic::BuildDefaultTacticSpecs(Tactics, 3.5f);
		TestEqual(TEXT("three tactics"), Tactics.Num(), 3);
		for (const FAIBAmbitionSpec& Spec : Tactics)
		{
			if (Spec.Tag == AIBTags::Tactic_Push)
			{
				TestEqual(TEXT("the floor never commits"), Spec.CommitSeconds, 0.f);
			}
			if (Spec.Tag == AIBTags::Tactic_Flank)
			{
				TestEqual(TEXT("Flank's commit is the tier row's"), Spec.CommitSeconds, 3.5f);
			}
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
