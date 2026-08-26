#pragma once

#include "CoreMinimal.h"
#include "Core/AIBTypes.h"
#include "Math/RandomStream.h"

/**
 * PHASE 4, skill 3 — grenades, worldless: WHEN a grenade is worth it, recognised
 * progressively by level. The conventions block in AIBSkillProfile.h binds this file.
 *
 * The recognition ladder (each level sees everything below it):
 *   Novice   — never throws deliberately, and CANNOT EVADE an incoming blast: the
 *              capability gate is design ("you can catch it with one"), not a number.
 *   Trained  — the OPENER: a visible target inside the throwing band, grenades in
 *              pocket. Can evade.
 *   Skilled  — + the FINISHER: pressure is landing (recent damage DEALT is high —
 *              enemy vitals are NOT perceivable, F3; our own damage history is the
 *              honest proxy a human also plays by).
 *   Expert   — + AREA DENIAL: sight was just lost but the memory is fresh — the throw
 *              at where they went, the read that looks like prediction.
 * Every fact consumed comes from FAIBFacts — matured, envelope-bounded. The executor's
 * grenade task turns a call into Verb_Grenade with the face task already steering.
 */

/** What the policy recognised this moment; None means hold the grenade. */
enum class EAIBGrenadeCall : uint8
{
	None,
	Opener,
	Finisher,
	AreaDenial
};

/** Per-life state: the consider cadence (throws are decisions, not per-tick rolls). */
struct AIBOT_API FAIBGrenadeState
{
	double NextConsiderAtSeconds = 0.0;
};

struct AIBOT_API FAIBGrenadePolicy
{
	// -- the ladder --
	/** THE CAPABILITY GATE: may this level evade a matured incoming blast at all?
	 *  Novice = false, at any tuning value. The controller's blast response consults
	 *  this before any dodge movement exists. */
	static bool CanEvadeBlast(EAIBCompetence Level);
	/** The throwing band (uu) — outside it even a recognised moment is a pass. */
	static float ThrowBandMinUU(EAIBCompetence Level);
	static float ThrowBandMaxUU(EAIBCompetence Level);
	/** Seconds between considerations — recognition is a glance cadence, not a tick. */
	static float ConsiderSeconds(EAIBCompetence Level);

	/**
	 * The one step: consult the facts on the consider cadence and return the call.
	 * Requires GrenadeCount > 0 and the level's recognition for the situation; pure
	 * over facts + Rng.
	 */
	static EAIBGrenadeCall Consider(FAIBGrenadeState& State, const FAIBFacts& Facts,
		EAIBCompetence Level, FRandomStream& Rng, double NowSeconds);
};
