#pragma once

#include "CoreMinimal.h"

/**
 * WHO AM I FIGHTING — the question this module could not previously answer.
 *
 * Until 1 Sep the sensorium kept ONE target slot and the newest matured sighting simply
 * overwrote it. There was no score, no comparison, no persistence and no switch rule, and
 * damage could not name a target at all. The founder described the two halves of that
 * from opposite ends on the same day: "if you shoot the AI... it's not knowing who is
 * shooting him", and "there should be a score... I would try to be as persistent as
 * possible, realistically, to kill this target... but if he managed to run away, or is
 * there any other immediate threat facing me right now, actually be able to safely change
 * targets."
 *
 * Both are one missing layer: an OPINION about which believed enemy matters most.
 *
 * WHAT THIS DOES NOT DO, and must never do (FAIRPLAY F2/F3):
 *  - it does not create belief. Every candidate got here through a matured stimulus on
 *    the reaction clock. Scoring re-ranks what the bot already believes in; it cannot
 *    conjure an enemy the bot has not perceived.
 *  - it does not read enemy vitals. There is no health term and there must not be — no
 *    human reads a health fraction off a silhouette, and the 26-Aug ruling stands.
 *  - it does not read live positions. Distances come from the sensorium's once-per-pump
 *    beliefs, which freeze at the last seen spot exactly as they did before.
 *
 * Pure and worldless: no actors, no engine, no time source of its own. Everything is a
 * float already measured by the caller, which is what lets AIBot.Sim.TargetPolicy pin the
 * hysteresis and the priority order without a world.
 */
struct AIBOT_API FAIBTargetScoreInput
{
	/** In sight RIGHT NOW (a matured gain, no matured loss since). */
	bool bSightCurrent = false;

	/** Distance to the believed position. Negative = unknown, which scores no proximity
	 *  rather than infinite proximity — the facts convention, kept. */
	float DistanceUU = -1.f;

	/** Since the last matured perception of this actor. Negative = never perceived. */
	float SecondsSinceSeen = -1.f;

	/** Since this actor last damaged us. Negative = never. */
	float SecondsSinceDamagedMe = -1.f;

	/** PHASE 12: allied claims already on this target — SELF EXCLUDED, from the claims
	 *  board only (negative-only teammate intent: it lowers, never conjures). Scales the
	 *  sight/proximity/freshness sum by 1/(1+n); the THREAT term is added after, unscaled
	 *  — a bot is never turned away from the man shooting it. */
	int32 AlliesOnTarget = 0;
};

struct AIBOT_API FAIBTargetPolicy
{
	/** The four terms, summed. Incumbency is NOT included — it is applied by Choose, so
	 *  that a spec can compare two candidates on merit alone. */
	static float Score(const FAIBTargetScoreInput& In, float MemoryWindowSeconds);

	/**
	 * The winner among candidates, with hysteresis.
	 *
	 * @param Incumbent  index of the target currently held, or INDEX_NONE
	 * @return the index to hold, or INDEX_NONE when the list is empty
	 *
	 * The incumbent keeps the slot unless a challenger beats it by both the persistence
	 * bonus and the switch margin. An incumbent that is no longer in the list has already
	 * been dropped by the caller (dead, or aged out of memory) and simply is not defended.
	 */
	static int32 Choose(const TArray<FAIBTargetScoreInput>& Candidates, int32 Incumbent,
		float MemoryWindowSeconds);
};
