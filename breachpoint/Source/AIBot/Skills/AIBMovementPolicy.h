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

	/**
	 * The one step, called at task tick rate while engaging: advances the decision
	 * clock and returns the current intent. Pure over its inputs + Rng.
	 */
	static EAIBStrafeIntent StepStrafe(FAIBMovementState& State, EAIBCompetence Level,
		FRandomStream& Rng, double NowSeconds);
};
