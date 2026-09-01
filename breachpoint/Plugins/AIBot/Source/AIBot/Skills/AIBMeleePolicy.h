#pragma once

#include "CoreMinimal.h"
#include "Core/AIBTypes.h"
#include "Math/RandomStream.h"

/**
 * PHASE 4, skill 4 — melee, worldless: range recognition and commit. Knowing when the
 * correct answer at arm's length is the melee, and when backing up to shoot reads as
 * broken. The conventions block in AIBSkillProfile.h binds this file.
 *
 * Two numbers make the whole read: the range at which a level RECOGNISES melee as the
 * answer (Novice only realises point-blank; Expert reads the closing fight early), and
 * the recognition DELAY — the beat between the range being true and the bot acting on
 * it, which is what keeps point-blank from being instantly lethal (the same reasoning
 * as R11's reaction gate, applied to the knife). Commit is binary: once this policy
 * says melee, the executor's melee task presses Verb_Melee and does not also shoot.
 */

/** Per-life state: when the range condition became continuously true. */
struct AIBOT_API FAIBMeleeState
{
	/** <0 = not currently in recognised range; else the time the condition started. */
	double InRangeSinceSeconds = -1.0;

	/** Absolute time before which no swing lands — CONTROLLER-owned, like every policy
	 *  throttle, because StateTree instance data re-initialises on branch re-entry and a
	 *  per-task countdown reset on every belief blink throttled nothing (W-REVIEW P4+5
	 *  H2 — the grenade-cooldown lesson, applied to the swing). Absolute, not a
	 *  countdown, so nothing has to remember to decay it. */
	double NextSwingAtSeconds = 0.0;
};

struct AIBOT_API FAIBMeleePolicy
{
	// -- the ladder --
	/** Range (uu) inside which this level recognises the melee answer. */
	/** bEmptyHanded widens the RECOGNITION range — a bot with nothing to shoot reads a
	 *  melee fight from further out, because there is nothing else it could be doing.
	 *  It deliberately does NOT shorten RecognitionDelaySeconds: that delay is the
	 *  FAIRPLAY-bound humaniser whose top rung IS the module floor, and a dry Spartan
	 *  reacting faster than a loaded one is the opposite of what the fairness pass
	 *  wanted. Decide sooner in SPACE, never faster in TIME. */
	static float CommitRangeUU(EAIBCompetence Level, bool bEmptyHanded = false);
	/** The beat between range-true and acting. Expert short, Novice long. */
	static float RecognitionDelaySeconds(EAIBCompetence Level);

	/**
	 * The one step, at task tick rate: true when the melee is the answer NOW —
	 * a VISIBLE target has been inside the level's commit range for at least the
	 * level's recognition delay. DistToTargetUU < 0 means unknown (facts convention)
	 * and always answers false. Pure; no randomness needed — the delay IS the
	 * humaniser here.
	 */
	static bool ShouldMelee(FAIBMeleeState& State, float DistToTargetUU,
		bool bTargetVisible, EAIBCompetence Level, double NowSeconds,
		bool bEmptyHanded = false);
};
