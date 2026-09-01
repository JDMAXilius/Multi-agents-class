#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Skills/AIBWeaponPolicy.h"

/**
 * THE DEAD END, pinned (founder, 1 Sep: "they tend to have not a weapon on it").
 *
 * The bug this spec exists to make impossible: a bot whose weapon cycle stopped on the
 * host's null Unarmed slot could never restart it. The reset asked only "is the hand the
 * best answer here", which an empty hand can never be, so the press budget never refilled
 * and the bot was unarmed for the rest of its life. It was four conditions inline in a
 * task function — untestable, and therefore untested for as long as it existed.
 *
 * Pin 1 is the exact failing state and would have caught it on the day it was written.
 */
BEGIN_DEFINE_SPEC(FAIBWeaponPolicySpec, "AIBot.Sim.WeaponPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr int32 Cap = 5;

END_DEFINE_SPEC(FAIBWeaponPolicySpec)

void FAIBWeaponPolicySpec::Define()
{
	Describe("the dead end", [this]()
	{
		It("keeps cycling with an empty hand even after the budget is spent", [this]()
		{
			// THE EXACT BUG: holding the null slot, a working weapon in the pouch, and the
			// press cap already reached. The old code stopped here, forever.
			const FAIBSwapDecision D = FAIBWeaponPolicy::Decide(
				/*bCanFight*/ false, /*bHasUsable*/ true, /*bIsBestHere*/ false, Cap, Cap);
			TestTrue(TEXT("still cycles"), D.bCycle);
			TestTrue(TEXT("and says why"), D.bEmptyHanded);
			TestFalse(TEXT("never settled while holding nothing"), D.bSettled);
		});

		It("keeps cycling with an empty hand at ANY press count", [this]()
		{
			for (int32 Presses = 0; Presses <= Cap * 4; ++Presses)
			{
				TestTrue(*FString::Printf(TEXT("cycles at %d presses"), Presses),
					FAIBWeaponPolicy::Decide(false, true, false, Presses, Cap).bCycle);
			}
		});

		It("does not call an empty hand settled even if it is somehow 'best'", [this]()
		{
			// Defensive: if the adapter ever answers true for a hand that cannot fight,
			// settling on it would recreate the dead end from the other side.
			const FAIBSwapDecision D = FAIBWeaponPolicy::Decide(false, true, true, 2, Cap);
			TestFalse(TEXT("not settled"), D.bSettled);
		});
	});

	Describe("a dry loadout", [this]()
	{
		It("stops cycling — this is what the cap was always for", [this]()
		{
			// Nothing carried can fight. Spinning the wheel achieves nothing and a bot
			// doing it forever never swings and never throws. This case is the ONLY one
			// the budget was ever meant to stop, and stating it properly is the repair.
			for (int32 Presses = 0; Presses <= Cap; ++Presses)
			{
				TestFalse(*FString::Printf(TEXT("no cycle at %d presses"), Presses),
					FAIBWeaponPolicy::Decide(false, false, false, Presses, Cap).bCycle);
			}
		});
	});

	Describe("a preference", [this]()
	{
		It("cycles while under budget and gives up at it", [this]()
		{
			TestTrue(TEXT("under budget"),
				FAIBWeaponPolicy::Decide(true, true, false, Cap - 1, Cap).bCycle);
			TestFalse(TEXT("at budget"),
				FAIBWeaponPolicy::Decide(true, true, false, Cap, Cap).bCycle);
			TestFalse(TEXT("past budget"),
				FAIBWeaponPolicy::Decide(true, true, false, Cap + 3, Cap).bCycle);
		});

		It("is never confused with the dead end", [this]()
		{
			// A fightable hand is a preference by construction, so the cap can never
			// apply to an empty one. The two arms are mutually exclusive on bCanFight.
			TestFalse(TEXT("a working hand is not empty-handed"),
				FAIBWeaponPolicy::Decide(true, true, false, 0, Cap).bEmptyHanded);
		});

		It("settles on the right weapon and stops", [this]()
		{
			const FAIBSwapDecision D = FAIBWeaponPolicy::Decide(true, true, true, 3, Cap);
			TestFalse(TEXT("no cycle"), D.bCycle);
			TestTrue(TEXT("settled"), D.bSettled);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
