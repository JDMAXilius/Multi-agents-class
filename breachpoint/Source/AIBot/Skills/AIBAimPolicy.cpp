#include "Skills/AIBAimPolicy.h"

/**
 * F4 made concrete. The bot's aim carries ONE error — a direction and a magnitude drawn
 * at a moment, decaying linearly to zero over the level's correction time. Between draws
 * nothing is rolled: two asks at the same instant get the same answer, so a shot is never
 * a dice roll on a perfect solution. The only events that draw are (a) a target switch,
 * which costs a big settle by construction, and (b) the wander cadence expiring.
 *
 * THE LADDER, and why these numbers:
 * - ErrorConeDegrees is the cone HALF-ANGLE a fresh draw may land inside (the header's
 *   word). 7.0 / 4.0 / 2.2 / 1.2 across Novice..Expert: at a 1500uu duel range a Novice's
 *   worst draw is ~184uu off centre (a clean miss past a torso), an Expert's ~31uu (a
 *   limb hit instead of a head). Roughly halving per rung keeps the felt gap wide without
 *   ever reaching zero — an Expert still drifts.
 * - CorrectSeconds is the settle. 1.6 / 1.1 / 0.7 / 0.45. Every rung sits ABOVE the F1
 *   reaction floor (0.20) by a comfortable margin, so no level's aim resolves faster than
 *   its own perception could have started it.
 * - RedrawSeconds is the wander cadence, and it INCREASES with competence
 *   (1.2 / 1.5 / 1.8 / 2.2): a better aim wanders less often, not merely less far. That
 *   pairing has a deliberate consequence at the bottom rung — a Novice's cadence (1.2) is
 *   SHORTER than its settle (1.6), so a Novice never fully arrives: it is always partway
 *   through correcting an error that has already been replaced. From Trained up, the
 *   settle completes before the next wander, so those bots do reach the belief point and
 *   hold it. That is the whole competence story in two numbers per rung.
 *
 * Worldless: positions, level, time and the stream come in; a point goes out.
 */

float FAIBAimPolicy::ErrorConeDegrees(EAIBCompetence Level)
{
	switch (Level)
	{
	case EAIBCompetence::Novice:  return 7.0f;
	case EAIBCompetence::Skilled: return 2.2f;
	case EAIBCompetence::Expert:  return 1.2f;
	case EAIBCompetence::Trained:
	default:                      return 4.0f; // unknown rung degrades to average, never to perfect
	}
}

float FAIBAimPolicy::CorrectSeconds(EAIBCompetence Level)
{
	switch (Level)
	{
	case EAIBCompetence::Novice:  return 1.6f;
	case EAIBCompetence::Skilled: return 0.7f;
	case EAIBCompetence::Expert:  return 0.45f;
	case EAIBCompetence::Trained:
	default:                      return 1.1f;
	}
}

float FAIBAimPolicy::RedrawSeconds(EAIBCompetence Level)
{
	switch (Level)
	{
	case EAIBCompetence::Novice:  return 1.2f;
	case EAIBCompetence::Skilled: return 1.8f;
	case EAIBCompetence::Expert:  return 2.2f;
	case EAIBCompetence::Trained:
	default:                      return 1.5f;
	}
}

namespace
{
	/**
	 * A unit vector perpendicular to Axis, chosen without randomness. Crossing with Up is
	 * the stable choice for the near-horizontal aim lines this policy sees; a near-vertical
	 * axis makes that cross degenerate, so it falls back to Forward, which cannot also be
	 * parallel to a vertical axis.
	 */
	FVector PerpendicularTo(const FVector& Axis)
	{
		FVector Perp = FVector::CrossProduct(Axis, FVector::UpVector);
		if (Perp.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			Perp = FVector::CrossProduct(Axis, FVector::ForwardVector);
		}
		return Perp.GetSafeNormal();
	}

	/** A uniformly random unit direction in the plane perpendicular to Axis. */
	FVector DrawErrorDirection(const FVector& Axis, FRandomStream& Rng)
	{
		const FVector PerpA = PerpendicularTo(Axis);
		const FVector PerpB = FVector::CrossProduct(Axis, PerpA).GetSafeNormal();
		// Degrees, not radians, so no circle constant is spelled out by hand.
		const float AngleRadians = FMath::DegreesToRadians(Rng.FRandRange(0.f, 360.f));
		const FVector Dir = PerpA * FMath::Cos(AngleRadians) + PerpB * FMath::Sin(AngleRadians);
		return Dir.GetSafeNormal();
	}
}

FVector FAIBAimPolicy::StepAimPoint(FAIBAimState& State, const FVector& EyeLocation,
	const FVector& BeliefPoint, uint32 TargetId, EAIBCompetence Level,
	FRandomStream& Rng, double NowSeconds)
{
	const FVector ToBelief = BeliefPoint - EyeLocation;
	const float Range = ToBelief.Size();
	if (Range <= KINDA_SMALL_NUMBER)
	{
		// Degenerate: there is no aim line to be wrong about. Answer the belief and leave
		// the state untouched — the clocks keep running, and if the line comes back the
		// stale id (or the empty direction) reads as a fresh draw below.
		return BeliefPoint;
	}
	const FVector Axis = ToBelief / Range;

	const float HalfConeDegrees = ErrorConeDegrees(Level);

	// A never-drawn state has no direction; that is the first call, and it settles exactly
	// like a switch (the bot has just acquired someone).
	const bool bFirstDraw = State.ErrorDir.IsNearlyZero();
	const bool bTargetSwitch = bFirstDraw || TargetId != State.TargetId;
	const bool bWanderDue = NowSeconds >= State.NextRedrawAtSeconds;

	if (bTargetSwitch || bWanderDue)
	{
		// THE ANTI-FLICK: a switch draws from the OUTER half of the cone, so snapping to a
		// new head never buys a lucky near-zero error. A wander on the same target may draw
		// anywhere in the cone, including small.
		State.DrawnErrorDegrees = bTargetSwitch
			? Rng.FRandRange(HalfConeDegrees * 0.5f, HalfConeDegrees)
			: Rng.FRandRange(0.f, HalfConeDegrees);
		State.ErrorDir = DrawErrorDirection(Axis, Rng);
		State.DrawnAtSeconds = NowSeconds;
		State.NextRedrawAtSeconds = NowSeconds + static_cast<double>(RedrawSeconds(Level));
		State.TargetId = TargetId;
	}

	// The settle: linear decay of the DRAWN magnitude to zero across the correction time.
	// Between draws this is a pure function of elapsed time — no per-tick randomness, no
	// per-shot roll (F4).
	const float Correct = CorrectSeconds(Level);
	const float Elapsed = static_cast<float>(NowSeconds - State.DrawnAtSeconds);
	const float SettledFraction = Correct > SMALL_NUMBER
		? FMath::Clamp(Elapsed / Correct, 0.f, 1.f)
		: 1.f;
	const float CurrentErrorDegrees = State.DrawnErrorDegrees * (1.f - SettledFraction);
	if (CurrentErrorDegrees <= SMALL_NUMBER)
	{
		return BeliefPoint;
	}

	// The aim line moves between ticks (both parties move), so the stored direction is
	// re-orthonormalised against the CURRENT axis before it is used, and stored back —
	// otherwise the "perpendicular" offset slowly acquires a component along the line and
	// the realised angle stops matching the modelled one.
	FVector ErrorDir = State.ErrorDir - Axis * FVector::DotProduct(State.ErrorDir, Axis);
	ErrorDir = ErrorDir.GetSafeNormal();
	if (ErrorDir.IsNearlyZero())
	{
		// The stored direction has become parallel to the new axis: take any perpendicular
		// rather than a new draw, so a geometry accident never consumes the stream.
		ErrorDir = PerpendicularTo(Axis);
	}
	State.ErrorDir = ErrorDir;

	// Angular error, not a fixed offset: the miss subtends the same angle at every range,
	// so distant targets are proportionally harder exactly as they are for a human.
	const float OffsetLength = Range * FMath::Tan(FMath::DegreesToRadians(CurrentErrorDegrees));
	return BeliefPoint + ErrorDir * OffsetLength;
}
