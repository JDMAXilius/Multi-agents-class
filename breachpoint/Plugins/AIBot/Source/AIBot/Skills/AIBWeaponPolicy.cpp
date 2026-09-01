#include "Skills/AIBWeaponPolicy.h"

FAIBSwapDecision FAIBWeaponPolicy::Decide(bool bCanFight, bool bHasUsable, bool bIsBestHere,
	int32 Presses, int32 MaxPresses)
{
	FAIBSwapDecision Out;

	// THE DEAD END, first and unconditionally. Nothing in hand, something in the pouch:
	// there is a correct answer one press away and no reason on earth to stop looking for
	// it. Explicitly NOT budgeted — the cap is a patience limit on a preference, and a bot
	// with empty hands is not being fussy.
	Out.bEmptyHanded = !bCanFight && bHasUsable;

	// THE PREFERENCE. Budgeted, because "nothing carried suits this range" is a real
	// answer and a bot that kept spinning the wheel over it would never fire again. Note
	// this arm requires bCanFight: a bot with empty hands takes the arm above, so the two
	// can never both be true and the cap can never apply to the dead end.
	const bool bWrongRanged = bCanFight && !bIsBestHere && Presses < MaxPresses;

	Out.bCycle = Out.bEmptyHanded || bWrongRanged;

	// SETTLED needs BOTH halves. Something that works AND the right something. The old
	// reset asked only the second, which an empty hand can never satisfy — so the budget
	// never refilled and the dead end was permanent. Asking both is the repair.
	Out.bSettled = bCanFight && bIsBestHere;
	return Out;
}
