#pragma once

#include "CoreMinimal.h"
#include "Core/AIBTypes.h"
#include "Math/RandomStream.h"

/**
 * PHASE 4, skill 1 — combat movement, worldless: strafe cadence and juke ratio per
 * competence. Returns movement INTENTS; the executor's strafe task turns them into
 * lateral movement input. The conventions block in AIBSkillProfile.h binds this file.
 *
 * The read this buys (the host's R9 lesson, generalised): a bot that stands perfectly
 * still while firing is the single loudest tell that it is not a person. A Novice
 * barely strafes; an Expert fights sideways and JUKES — reverses direction off-cadence
 * so the rhythm cannot be tracked. The juke is what separates rungs 3 and 4: same
 * cadence numbers, different predictability.
 */

/** The lateral intent for this moment of the fight. */
enum class EAIBStrafeIntent : uint8
{
	Hold,   // stand — aiming beats moving at this level, or the draw said pause
	Left,
	Right
};

/** Per-life strafe state; the controller owns one, the strafe task hands it in. */
struct AIBOT_API FAIBMovementState
{
	EAIBStrafeIntent Current = EAIBStrafeIntent::Hold;
	double NextDecisionAtSeconds = 0.0;

	/** The leg the strafe task last ACTUATED (its NextDecisionAtSeconds stamp) — here and
	 *  not in task instance data, because instance data re-initialises on every Engage
	 *  re-entry and a reset stamp re-actuated the SAME leg once per belief blink, walking
	 *  the bot further per leg than any rung authorises (W-REVIEW P4+5 H1). */
	double LastActuatedLegStamp = 0.0;

	/** AIB10's opportunity instrument: the out-of-gate HOLD as a SPELL, not a tick. The
	 *  first measurement logged the hold every Tick and the leg once per leg, and dividing
	 *  the two produced 182:1 — frames over legs, a number with no meaning. Edge state
	 *  here (per-life, survives Engage re-entry like the stamp above) lets the task log
	 *  one line when the spell starts and one with the DURATION when it ends, so the
	 *  re-measure can state the true split: seconds denied vs seconds stepping. */
	bool bStrafeOutsideGate = false;
	double StrafeOutsideSinceSeconds = 0.0;

	/** Did the leg just drawn REVERSE the previous one? The strafe task reads this to put
	 *  an evasive hop on a defending bot's direction changes. Recorded here rather than
	 *  returned, because the juke is a property of the LEG the policy drew and the task
	 *  actuates that leg on a later tick.
	 *
	 *  Riding the juke keeps the hop capability-shaped for free: JukeChance is 0 below
	 *  Skilled, so a Novice cannot juke and therefore cannot hop, and no new tier lever
	 *  is introduced (R28). */
	bool bLastLegWasJuke = false;
};

struct AIBOT_API FAIBMovementPolicy
{
	// -- the ladder --
	/** Chance (0..1) a decision window strafes at all. Novice ~0 — it stands. */
	static float StrafeChance(EAIBCompetence Level);
	/** Bounds of one strafe leg's duration; the draw lands between them. */
	static float StrafeLegSecondsMin(EAIBCompetence Level);
	static float StrafeLegSecondsMax(EAIBCompetence Level);
	/** Chance (0..1) a new leg REVERSES the previous direction early — the juke.
	 *  Capability-shaped: below Skilled this is 0 at any tuning value. */
	static float JukeChance(EAIBCompetence Level);

	/** Odds that ONE defending strafe leg leaves the ground. Retreat's band only — Engage's
	 *  footwork never consults it, so every strafe measurement in the AIB tickets stays
	 *  comparable.
	 *
	 *  Its own lever rather than a rider on JukeChance: the first cut rode the juke, which is
	 *  0.00 below Skilled, and the evasive jump was therefore impossible at Marine — the tier
	 *  actually played. Measured 0 hops across a full match before this existed. */
	static float HopChance(EAIBCompetence Level);

	/**
	 * The one step, called at task tick rate while engaging: advances the decision
	 * clock and returns the current intent. Pure over its inputs + Rng.
	 */
	static EAIBStrafeIntent StepStrafe(FAIBMovementState& State, EAIBCompetence Level,
		FRandomStream& Rng, double NowSeconds);
};
