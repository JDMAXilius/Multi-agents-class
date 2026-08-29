#include "Skills/AIBMovementPolicy.h"

/**
 * PHASE 4, skill 1 — the strafe ladder made concrete. The conventions block in
 * AIBSkillProfile.h binds this file: worldless, constants as static functions over
 * EAIBCompetence, per-life state owned by the caller, every draw through the caller's
 * seeded stream, time in as a double.
 *
 * WHY THESE NUMBERS. The rungs are chosen so the difference is READABLE ACROSS A ROOM,
 * not just in a table — the read the whole skill buys is "that one is a rookie, that one
 * is dangerous", and a ladder whose rungs sit within each other's noise buys nothing:
 *
 *   - StrafeChance 0.05 / 0.40 / 0.75 / 0.95. A Novice essentially plants its feet (the
 *     1-in-20 is deliberate: never-ever reads as a statue, which is its own tell). A
 *     Trained bot moves about as often as it stands. Skilled and Expert fight sideways
 *     by default, the Expert almost always.
 *   - Leg seconds 1.20-2.00 / 0.80-1.60 / 0.55-1.20 / 0.35-0.90. The cadence TIGHTENS as
 *     competence rises: a slow, long leg is trivially led by a human aimer, a short one
 *     is not. Bounds stay wide at every rung so the cadence itself is never a metronome.
 *   - JukeChance 0.00 / 0.00 / 0.25 / 0.50. CAPABILITY-SHAPED, per the roadmap's rule
 *     that levels gate capabilities and not merely numbers: below Skilled the reversal
 *     bias is EXACTLY zero, so no tier retune can hand a rookie an Expert's tell-free
 *     rhythm. A Skilled bot reverses off-cadence a quarter of the time; an Expert half.
 *
 * The four rungs live in one table so the ladder reads in one screen and Phase 8's tier
 * system retunes the ROW that selects a level, never these lines.
 */

namespace
{
	/** One rung of the ladder: everything the strafe decision needs at that competence. */
	struct FAIBStrafeRung
	{
		float Chance;   // odds a decision window strafes at all
		float LegMin;   // shortest leg the draw may land on, seconds
		float LegMax;   // longest, seconds
		float Juke;     // odds a new leg reverses the previous direction
		float Hop;      // odds a DEFENDING leg leaves the ground (Retreat only)
		float Dash;     // odds a strafe leg spends the dash, if it is off cooldown
	};

	// HOP, the last column, is NOT zero at the bottom two rungs — and that is the whole
	// point of it existing (founder, 28 Aug). The first cut rode the hop on Juke, which is
	// 0.00 below Skilled, so the evasive jump was structurally unreachable at Marine — the
	// tier actually being played. Measured: 9 defend stand-downs, 0 hops, in one match.
	//
	// Still capability-shaped, still monotone: a Novice hops sometimes and clumsily, an
	// Expert hops often. It is its own lever because it is its own behaviour — evading a
	// tracking aim — and not a side effect of changing direction.
	// DASH, the last column. Nonzero everywhere for the same reason Hop is — a behaviour the
	// low tiers can never show is a behaviour most players never see, since Marine is the
	// tier actually played. The real rate limiter is not this roll but the bot's own 3.5s
	// throttle: even an Expert rolling 0.35 every leg can only spend one dash per throttle
	// window, so these numbers shape WHO dashes eagerly, not how often anyone can.
	constexpr FAIBStrafeRung NoviceRung  { 0.05f, 1.20f, 2.00f, 0.00f, 0.20f, 0.10f };
	constexpr FAIBStrafeRung TrainedRung { 0.40f, 0.80f, 1.60f, 0.00f, 0.30f, 0.18f };
	constexpr FAIBStrafeRung SkilledRung { 0.75f, 0.55f, 1.20f, 0.25f, 0.45f, 0.28f };
	constexpr FAIBStrafeRung ExpertRung  { 0.95f, 0.35f, 0.90f, 0.50f, 0.60f, 0.40f };

	/** Out-of-range asks answer Trained — the same degrade-to-average rule the skill
	 *  profile's Level() query uses, so a future enum entry is average, never superhuman. */
	const FAIBStrafeRung& Rung(EAIBCompetence Level)
	{
		switch (Level)
		{
		case EAIBCompetence::Novice:  return NoviceRung;
		case EAIBCompetence::Skilled: return SkilledRung;
		case EAIBCompetence::Expert:  return ExpertRung;
		case EAIBCompetence::Trained:
		default:                      return TrainedRung;
		}
	}

	/** A 0..1 roll. FRandRange is the module's proven draw (Perception/AIBSensorium). */
	float Roll(FRandomStream& Rng)
	{
		return Rng.FRandRange(0.f, 1.f);
	}
}

float FAIBMovementPolicy::StrafeChance(EAIBCompetence Level)
{
	return Rung(Level).Chance;
}

float FAIBMovementPolicy::StrafeLegSecondsMin(EAIBCompetence Level)
{
	return Rung(Level).LegMin;
}

float FAIBMovementPolicy::StrafeLegSecondsMax(EAIBCompetence Level)
{
	return Rung(Level).LegMax;
}

float FAIBMovementPolicy::JukeChance(EAIBCompetence Level)
{
	return Rung(Level).Juke;
}

float FAIBMovementPolicy::HopChance(EAIBCompetence Level)
{
	return Rung(Level).Hop;
}

float FAIBMovementPolicy::DashChance(EAIBCompetence Level)
{
	return Rung(Level).Dash;
}

EAIBStrafeIntent FAIBMovementPolicy::StepStrafe(FAIBMovementState& State, EAIBCompetence Level,
	FRandomStream& Rng, double NowSeconds)
{
	// STABLE LEGS. Inside the current leg this is a pure read: no draw, no rewrite of the
	// deadline. The task calls this every tick, and a per-tick reroll would turn the whole
	// ladder into 60Hz noise that no cadence — and no tell — survives. A fresh per-life
	// state has a 0.0 deadline, which is behind any clock, so the first step decides.
	if (NowSeconds < State.NextDecisionAtSeconds)
	{
		return State.Current;
	}

	const EAIBStrafeIntent Previous = State.Current;

	if (Roll(Rng) >= StrafeChance(Level))
	{
		// This window stands. Note the direction memory goes with it: a bot that stopped
		// has nothing to reverse OFF, so the leg after a hold is a fresh pick, not a juke.
		State.Current = EAIBStrafeIntent::Hold;
		State.bLastLegWasJuke = false;
	}
	else
	{
		// THE JUKE. Reversing the previous direction is what makes the rhythm untrackable;
		// with JukeChance 0 (Novice, Trained) this branch cannot be taken at any roll,
		// because FRandRange never returns below its floor. That is the capability gate.
		const bool bHasDirection = Previous != EAIBStrafeIntent::Hold;
		const bool bJuke = bHasDirection && Roll(Rng) < JukeChance(Level);
		State.bLastLegWasJuke = bJuke;
		if (bJuke)
		{
			State.Current = (Previous == EAIBStrafeIntent::Left)
				? EAIBStrafeIntent::Right
				: EAIBStrafeIntent::Left;
		}
		else
		{
			// No juke: an even pick. Even, deliberately — a bot that favours one side
			// drifts across the arena over a fight and reads as broken pathing.
			State.bLastLegWasJuke = false;
			State.Current = (Roll(Rng) < 0.5f)
				? EAIBStrafeIntent::Left
				: EAIBStrafeIntent::Right;
		}
	}

	// One leg drawn per decision, holds included: a stand is as timed as a strafe, or the
	// bot would re-ask every tick while standing and stutter the moment its odds came up.
	State.NextDecisionAtSeconds = NowSeconds
		+ static_cast<double>(Rng.FRandRange(StrafeLegSecondsMin(Level), StrafeLegSecondsMax(Level)));

	return State.Current;
}
