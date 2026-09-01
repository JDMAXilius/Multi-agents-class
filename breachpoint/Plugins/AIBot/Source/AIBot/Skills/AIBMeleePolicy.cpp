#include "Skills/AIBMeleePolicy.h"

#include "Core/AIBTypes.h"

/**
 * The two numbers, and nothing else. Both ladders are static functions over the
 * competence enum (the policy convention in AIBSkillProfile.h) so the whole read fits on
 * one screen and Phase 8 retunes through the tier table rather than by editing them.
 * Out-of-range enum asks answer TRAINED — the same degrade-to-average rule the profile's
 * Level() uses, so a future rung is average, never superhuman and never a crash.
 */

float FAIBMeleePolicy::CommitRangeUU(EAIBCompetence Level, bool bEmptyHanded)
{
	// WHY THESE NUMBERS: the weapon's reach is arm's length (~150uu with the lunge), but
	// this is a RECOGNITION range, not a reach — the distance at which a level reads "this
	// is a melee fight now" and lets the executor close the last step. Novice 160 sits
	// barely past reach: it only realises point-blank, which is exactly the tell of a weak
	// bot. Each rung adds ~60uu, and Expert 320 is about two strides — roughly half a
	// second of a sprinting closer — so an Expert reads the closing fight while it is still
	// closing and is already swinging when it arrives, while a Novice is still shooting.
	//
	// EMPTY-HANDED DOUBLES IT (founder, 1 Sep: "in case that doesn't have ammo, then the
	// melee attack should be"). A bot with nothing that can fire has no competing use for
	// the distance, so it should read the melee fight from roughly a second of closing
	// out rather than half of one. It is a RANGE change only — the recognition delay does
	// not move, so a dry bot decides sooner in space and still swings on the same human
	// beat.
	float Range = 220.f;
	switch (Level)
	{
	case EAIBCompetence::Novice:  Range = 160.f; break;
	case EAIBCompetence::Trained: Range = 220.f; break;
	case EAIBCompetence::Skilled: Range = 280.f; break;
	case EAIBCompetence::Expert:  Range = 320.f; break;
	default:                      Range = 220.f; break;
	}
	return bEmptyHanded ? Range * AIB::EmptyHandedMeleeRangeFactor : Range;
}

float FAIBMeleePolicy::RecognitionDelaySeconds(EAIBCompetence Level)
{
	// The beat between range-true and acting — this skill's R11-style humaniser, and the
	// only thing keeping point-blank from being instantly lethal. FAIRPLAY F1 binds it:
	// tiers draw at or above AIB::MinReactionSeconds, never below, so the top rung IS the
	// module floor rather than a hand-typed number that quietly undercuts it.
	switch (Level)
	{
	case EAIBCompetence::Novice:  return 0.60f;
	case EAIBCompetence::Trained: return 0.40f;
	case EAIBCompetence::Skilled: return 0.25f;
	case EAIBCompetence::Expert:  return AIB::MinReactionSeconds;
	default:                      return 0.40f;
	}
}

bool FAIBMeleePolicy::ShouldMelee(FAIBMeleeState& State, float DistToTargetUU,
	bool bTargetVisible, EAIBCompetence Level, double NowSeconds, bool bEmptyHanded)
{
	// The range condition, in one expression. A negative distance is the facts
	// convention's UNKNOWN and answers false — an unknown range is not a near range. The
	// ordering also makes a NaN distance fail both comparisons, so a poisoned fact can
	// only refuse the melee, never grant it.
	const bool bInRange = bTargetVisible
		&& DistToTargetUU >= 0.f
		&& DistToTargetUU <= CommitRangeUU(Level, bEmptyHanded);

	if (!bInRange)
	{
		// CONTINUOUS-range law: any break clears the clock. A target hopping in and out
		// of the commit range accumulates nothing, so the delay cannot be paid in
		// instalments across separate approaches — which is what would let a bot swing
		// the instant a juking player crossed the line for the third time.
		State.InRangeSinceSeconds = -1.0;
		return false;
	}

	if (State.InRangeSinceSeconds < 0.0)
	{
		// First tick of this continuous stretch: stamp the caller's clock. Nothing here
		// reads a clock of its own.
		State.InRangeSinceSeconds = NowSeconds;
	}

	// Commit at the edge and after it. A clock that ran backwards (a state carried into a
	// new life without a reset) yields a negative elapsed and simply refuses, which is the
	// safe answer for a stale stamp.
	const double Elapsed = NowSeconds - State.InRangeSinceSeconds;
	return Elapsed >= static_cast<double>(RecognitionDelaySeconds(Level));
}
