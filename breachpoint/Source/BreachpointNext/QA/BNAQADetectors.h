// THE RULES, IN ONE PLACE, WITH NO ENGINE IN THEM.
//
// Every question the adversarial QA probe asks about "is this broken?" is answered by a
// pure function in this header. Plain doubles and bools — no UObject, no FVector, no
// UE header of any kind — for one reason: the rule layer must be runnable, and provable,
// WITHOUT an editor. `assignments/09-adversarial-qa/tests/detector_tests.cpp` compiles
// this file with a stock C++ compiler and pushes every rule through both its firing case
// and its excuse case, so a grader with no Unreal install can still execute the logic
// they are grading.
//
// ONE SOURCE OF TRUTH. ABNAQAController (BNAdversarialAgent.cpp) does not re-decide any
// of this — it gathers engine state, converts it to plain numbers, and calls these
// functions. A rule proven here is the rule that ran in the game. If a threshold ever
// wants changing, it changes here and both the game and the tests move together; that
// asymmetry — a validator narrower or looser than the thing it validates — is the exact
// defect that bit this project in assignments #4, #6 and #8, and this header is the
// structural answer to it.
//
// EXCUSE POLICIES ARE PART OF THE RULE, not a caller's afterthought. Each predicate takes
// the facts that would legitimately explain the symptom (frozen, falling, respawning) and
// answers false when one applies. A detector without its excuses is a false-positive
// generator, and a QA report full of false positives is worse than no report.

#pragma once

namespace BNAQA
{
	// -- thresholds. Named, not inlined at the call site, so the report, the tests and
	//    the README can all quote the same numbers. -----------------------------------
	namespace Thresholds
	{
		constexpr double BelowKillZGraceS = 1.0;      // KillZ should destroy within a frame or two
		constexpr double StuckAfterS = 3.0;           // commanded + alive + unfrozen + still, this long
		constexpr double StuckSpeedUU = 10.0;         // "still" — under a walk, above float noise
		constexpr double SpeedTolerance = 1.75;       // ground speed past MaxWalkSpeed * this = cheat
		constexpr double TeleportUUPerSample = 1200.0;// per 100ms sample = 12,000 uu/s
		constexpr double HullMarginUU = 4000.0;       // outside PlayerStart hull + this = escaped
		constexpr double ProbeSeconds = 0.1;          // the sample cadence the numbers above assume
		constexpr double AttributeSlackUU = 1.0;      // rounding room before "above max" convicts
	}

	/** Is x a real number? Written by hand rather than pulled from <cmath> so this header
	 *  needs no include at all — and NaN is precisely the value that fails self-equality,
	 *  which is the whole trick. */
	inline bool IsFinite(double X)
	{
		return X == X && X < 1.0e300 && X > -1.0e300;
	}

	// -- 1. fell_out_of_world_alive ---------------------------------------------------
	/** KillZ exists to destroy what falls past it. Still breathing down there, past the
	 *  grace window, means the kill volume never fired — a hole in the world's floor.
	 *  Excuses: being dead (that is KillZ working), and the grace window itself (a frame
	 *  or two below the line is the volume taking its turn, not a defect). */
	inline bool FellOutOfWorldAlive(bool bAlive, double Z, double KillZ, double SecondsBelow)
	{
		return bAlive && Z < KillZ && SecondsBelow > Thresholds::BelowKillZGraceS;
	}

	// -- 2. escaped_playable_space ----------------------------------------------------
	/** STANDING ON GROUND outside the arena's intended footprint. The bounds are a
	 *  heuristic — the hull of the level's PlayerStarts, generously expanded — and the
	 *  report says so on every finding it produces.
	 *  Excuses: being airborne (falling past the edge is KillZ's case, not this one),
	 *  being dead, and any margin inside the expanded hull. The caller passes
	 *  bOutsideExpandedHull because the hull maths is FVector work; the RULE — that only
	 *  a grounded, living body outside the space convicts — lives here. */
	inline bool EscapedPlayableSpace(bool bAlive, bool bOnGround, bool bHullValid,
		bool bOutsideExpandedHull)
	{
		return bAlive && bOnGround && bHullValid && bOutsideExpandedHull;
	}

	// -- 3. stuck_state ---------------------------------------------------------------
	/** The game ACCEPTED a move order and the body goes nowhere. Not "the bot is idle" —
	 *  idle is not a defect — but "something is holding a commanded pawn in place".
	 *  Excuses: frozen (the match freeze is SUPPOSED to pin us — this is the excuse that
	 *  keeps warmup from generating a stuck finding every single round), dead, and having
	 *  no live move request at all. */
	inline bool StuckState(bool bAlive, bool bFrozen, bool bMoveCommanded, double Speed2D,
		double SecondsStill)
	{
		return bAlive && !bFrozen && bMoveCommanded
			&& Speed2D < Thresholds::StuckSpeedUU
			&& SecondsStill > Thresholds::StuckAfterS;
	}

	// -- 4. speed_violation -----------------------------------------------------------
	/** WALKING faster than the movement model says this body can walk.
	 *  Excuses: not being on the ground at all. Falling and grapple flight have their own
	 *  legal envelopes — gravity and root motion routinely exceed MaxWalkSpeed and are not
	 *  cheats — so the rule only convicts a body whose feet are down. Also excused: a
	 *  non-positive MaxWalkSpeed, which means we could not read the model rather than that
	 *  the model was broken. */
	inline bool SpeedViolation(bool bOnGround, double Speed2D, double MaxWalkSpeed)
	{
		return bOnGround && MaxWalkSpeed > 0.0
			&& Speed2D > MaxWalkSpeed * Thresholds::SpeedTolerance;
	}

	// -- 5. attribute_anomaly ---------------------------------------------------------
	/** The attribute set's own invariants: finite, non-negative, never above its own max.
	 *  GAS purity says only GameplayEffects may move these; this asks whether they ever
	 *  moved past their rails.
	 *  Excuse: a max of zero or less means the attribute is unconfigured or unreadable —
	 *  "I cannot see it" must never read as "it is broken", the same discipline the bot
	 *  adapter uses when it answers 1.0 for an unknowable vital. */
	inline bool AttributeAnomaly(double Health, double MaxHealth, double Shield, double MaxShield)
	{
		if (!IsFinite(Health) || !IsFinite(Shield))
		{
			return true;
		}
		if (Health < -1.0e-4 || Shield < -1.0e-4)
		{
			return true;
		}
		if (MaxHealth > 0.0 && Health > MaxHealth + Thresholds::AttributeSlackUU)
		{
			return true;
		}
		if (MaxShield > 0.0 && Shield > MaxShield + Thresholds::AttributeSlackUU)
		{
			return true;
		}
		return false;
	}

	// -- 6. teleport_discontinuity ----------------------------------------------------
	/** More ground covered in one sample than any legal mover here can cover.
	 *  Excuse: no previous sample to compare against. That is not pedantry — it is the
	 *  rule that keeps RESPAWNS from reading as teleports, because the controller drops
	 *  its sample chain on every possession. A validator that convicted a legal respawn
	 *  would bury the real finding in noise. */
	inline bool TeleportDiscontinuity(bool bHasPreviousSample, double StepUU)
	{
		return bHasPreviousSample && StepUU > Thresholds::TeleportUUPerSample;
	}

	// -- 7. acted_while_dead / input_during_freeze ------------------------------------
	/** A state gate gave way. State.Dead and State.Match.Frozen are both supposed to
	 *  refuse EVERY ability activation; the probe leans on both all run long, and this
	 *  answers whether anything got through.
	 *  Ordering is deliberate: dead outranks frozen, so a corpse frozen in the post-match
	 *  reports the more serious of the two rather than both. */
	enum class EGateBreak { None, ActedWhileDead, InputDuringFreeze };

	inline EGateBreak GateBreak(bool bAbilityActivated, bool bDead, bool bFrozen)
	{
		if (!bAbilityActivated)
		{
			return EGateBreak::None;
		}
		if (bDead)
		{
			return EGateBreak::ActedWhileDead;
		}
		if (bFrozen)
		{
			return EGateBreak::InputDuringFreeze;
		}
		return EGateBreak::None;
	}
}
