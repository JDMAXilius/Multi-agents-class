#pragma once

#include "CoreMinimal.h"
#include "Core/AIBTypes.h"
#include "Math/RandomStream.h"

/**
 * PHASE 5 — the fifth skill of the combat dance: CONFIDENCE. (OUR design; the studied
 * studio never published theirs — provenance per the roadmap's honesty note.)
 *
 * Two worldless pieces, both owned by the controller and stepped at think cadence:
 *
 * THE DAMAGE LEDGER — momentum as two exponentially-decayed accumulators, damage taken
 * and damage dealt, in fractions of the OWNER's max health (the caller normalises; the
 * module cannot know a max). This is what finally gives FAIBFacts' damage-history
 * fields a source; the fields may exceed 1.0 in a burst, by contract.
 *
 * THE CONFIDENCE MODEL — a 0..1 judgment of "am I winning this fight", assembled from
 * facts a human also has (own health, momentum, weapon fitness, visible enemy count —
 * NEVER enemy vitals, F3), then passed through the level's JUDGMENT: a held misjudge
 * offset drawn on a cadence. A Novice misjudges fights — that is what a novice IS —
 * and its wrongness is CONSISTENT between draws (held, like the aim error), because
 * per-tick noise reads as jitter while a held wrong read reads as a bad call. Output
 * feeds one fact (ConfidenceNorm) that scales Engage up and Retreat down and back.
 */

/** Momentum: decayed damage-traded windows. Plain floats + times; worldless. */
struct AIBOT_API FAIBDamageLedger
{
	/** Fraction of the owner's max health this hit removed (may sum past 1). */
	void NoteTaken(float FractionOfMaxHealth, double NowSeconds);
	/** Fraction of the VICTIM's max health a hit we landed removed. */
	void NoteDealt(float FractionOfMaxHealth, double NowSeconds);

	float TakenNorm(double NowSeconds) const;
	float DealtNorm(double NowSeconds) const;

	void Reset();

	/** Half-life of the momentum window. One number, one site; specs pin the decay. */
	static constexpr float HalfLifeSeconds = 4.f;

private:
	float Decayed(float Value, double StampedAt, double Now) const;

	float Taken = 0.f;
	float Dealt = 0.f;
	double TakenStamp = 0.0;
	double DealtStamp = 0.0;
};

/** Per-life judgment state: the held misjudge offset and its redraw clock. */
struct AIBOT_API FAIBConfidenceState
{
	float MisjudgeOffset = 0.f;
	double NextJudgeAtSeconds = 0.0;
};

struct AIBOT_API FAIBConfidenceModel
{
	// -- the ladder (judgment QUALITY; the assessment formula is level-invariant) -----
	/** Held misjudge amplitude: the draw lands in [-A, +A]. Novice wide, Expert thin. */
	static float MisjudgeAmplitude(EAIBCompetence Level);
	/** Seconds a misjudge is HELD before re-judging. */
	static float ReJudgeSeconds(EAIBCompetence Level);

	/**
	 * The one step, at think cadence: assess the fight from the facts, apply the
	 * level's held misjudgment, return 0..1. Pure over facts + state + Rng. The facts'
	 * damage fields must already be filled (the ledger runs first).
	 */
	static float Step(FAIBConfidenceState& State, const FAIBFacts& Facts,
		EAIBCompetence Level, FRandomStream& Rng, double NowSeconds);

	/** The level-invariant TRUE assessment, exposed for specs: what a perfect judge
	 *  would read off these facts. Step = clamp(Assess + held offset). */
	static float Assess(const FAIBFacts& Facts);
};
