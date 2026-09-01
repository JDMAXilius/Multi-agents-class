#pragma once

#include "CoreMinimal.h"

/**
 * WHAT IS IN MY HANDS — the swap decision, extracted so it can be tested.
 *
 * It used to be four conditions inline in the engage task, and it was wrong in a way
 * nothing could catch: the host's carry contains a deliberate null Unarmed slot, the
 * cycle was capped at a few presses, and a cycle that stopped ON that slot could never
 * restart. `IsBestWeaponForRange` answers false there forever (the held thing is not the
 * best AND cannot fight), so the "settled" reset never fired, the press budget never
 * refilled, and the bot stayed empty-handed for the rest of its life. The founder saw it
 * from the outside on 1 Sep: "they tend to have not a weapon on it".
 *
 * The fix is not a bigger cap. It is noticing that CYCLING HAS TWO CAUSES with different
 * rules, and that the module was asking one question where it needed three:
 *
 *   bCanFight    - the weapon IN HAND works right now
 *   bHasUsable   - SOMETHING in the pouch works, anywhere, at any range
 *   bIsBestHere  - the hand is already the best answer for this range
 *
 * EMPTY-HANDED (!bCanFight && bHasUsable) is a dead end, and a dead end is not budgeted:
 * keep cycling regardless of the press count. WRONG-RANGED (bCanFight && !bIsBestHere) is
 * a preference, and preferences are budgeted — that is what the cap was always for, and
 * "my whole loadout is dry" (!bHasUsable) is the case it was meant to stop. Stating that
 * case properly is the whole repair.
 *
 * Pure and worldless on purpose: no avatar, no weapons, no engine. Three booleans and two
 * counts in, one decision out, so AIBot.Sim.WeaponPolicy can pin the dead end directly.
 */
struct AIBOT_API FAIBSwapDecision
{
	/** Press the cycle verb this tick (subject to the caller's own press cooldown). */
	bool bCycle = false;

	/** The reason, when cycling: a dead end rather than a preference. Uncapped, and the
	 *  caller logs it differently because the two mean very different things in a match. */
	bool bEmptyHanded = false;

	/** The hand is right: fightable, and the best answer for this range. The caller
	 *  refunds the press budget here — and note it requires bCanFight, which the old
	 *  single-condition reset did not, which is precisely why it could never release the
	 *  dead end it was supposed to release. */
	bool bSettled = false;
};

struct AIBOT_API FAIBWeaponPolicy
{
	/**
	 * @param bCanFight    the HELD weapon can fight now
	 * @param bHasUsable   ANY carried weapon can fight (the pouch question)
	 * @param bIsBestHere  the held weapon is the best carried answer at this range
	 * @param Presses      cycles already spent on this decision
	 * @param MaxPresses   the budget for a PREFERENCE (one lap of the carry)
	 */
	static FAIBSwapDecision Decide(bool bCanFight, bool bHasUsable, bool bIsBestHere,
		int32 Presses, int32 MaxPresses);
};
