#include "Perception/AIBTargetPolicy.h"

#include "Core/AIBTypes.h"

float FAIBTargetPolicy::Score(const FAIBTargetScoreInput& In, float MemoryWindowSeconds)
{
	float Total = 0.f;

	// SEEING beats remembering. Flat, not scaled: either the bot has eyes on or it does
	// not, and there is no partial credit for nearly seeing someone.
	if (In.bSightCurrent)
	{
		Total += AIB::TargetVisibleWeight;
	}

	// CLOSENESS, linear to the consider range. An unknown distance scores nothing rather
	// than everything — the facts convention's negative-is-unknown, and the safe
	// direction: a candidate we cannot place should not out-rank one we can.
	if (In.DistanceUU >= 0.f)
	{
		const float Near = 1.f - FMath::Clamp(In.DistanceUU / AIB::TargetConsiderRangeUU, 0.f, 1.f);
		Total += AIB::TargetProximityWeight * Near;
	}

	// FRESHNESS over the tier's own memory window, so a Recruit's short memory makes its
	// stale leads decay faster than a Spartan's without a second number anywhere.
	if (In.SecondsSinceSeen >= 0.f)
	{
		const float Window = FMath::Max(0.01f, MemoryWindowSeconds);
		const float Fresh = 1.f - FMath::Clamp(In.SecondsSinceSeen / Window, 0.f, 1.f);
		Total += AIB::TargetFreshnessWeight * Fresh;
	}

	// PHASE 12 — TEAMMATES ALREADY ON HIM (AIB23 W-AUDIT): ×1/(1+n) over the three terms
	// above, so a target two allies hold is worth a third to a third bot, and the pile
	// dissolves by score rather than by prohibition (F9: denial is a score, never a veto).
	Total /= 1.f + static_cast<float>(FMath::Max(In.AlliesOnTarget, 0));

	// WHO IS SHOOTING ME, decaying by halves. Exponential rather than linear because
	// being shot at is about the CURRENT exchange: it should dominate for a few seconds
	// and then stop mattering, not fade politely over the whole memory window. Added
	// AFTER the crowd divisor, unscaled: never turn a bot away from the man shooting it.
	if (In.SecondsSinceDamagedMe >= 0.f)
	{
		const float Halves = In.SecondsSinceDamagedMe / FMath::Max(0.01f, AIB::TargetThreatHalfLifeSeconds);
		Total += AIB::TargetThreatWeight * FMath::Pow(0.5f, Halves);
	}

	return Total;
}

int32 FAIBTargetPolicy::Choose(const TArray<FAIBTargetScoreInput>& Candidates, int32 Incumbent,
	float MemoryWindowSeconds)
{
	if (Candidates.Num() == 0)
	{
		return INDEX_NONE;
	}

	const bool bHasIncumbent = Candidates.IsValidIndex(Incumbent);

	// The best CHALLENGER — the incumbent never competes with itself, or the margin below
	// would be measured against a number that already contains the bonus.
	int32 BestOther = INDEX_NONE;
	float BestOtherScore = 0.f;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		if (i == Incumbent)
		{
			continue;
		}
		const float S = Score(Candidates[i], MemoryWindowSeconds);
		if (BestOther == INDEX_NONE || S > BestOtherScore)
		{
			BestOther = i;
			BestOtherScore = S;
		}
	}

	if (!bHasIncumbent)
	{
		// Nothing held: take the best on merit. This is also the path a bot takes the
		// instant it is shot with no target — the shooter is simply the only candidate
		// with a threat term, so it wins without any special case for damage anywhere.
		return BestOther;
	}

	// THE HYSTERESIS, and it is two knobs deliberately. The bonus is what the current
	// fight is worth; the margin is how much better a new one must LOOK before it is
	// worth abandoning. A challenger has to clear both. Without them a bot flips between
	// two enemies at similar range every time the numbers cross, which reads as a bot
	// that cannot make up its mind and is the classic failure of a nearest-enemy
	// selector — and it is exactly what the founder asked not to happen.
	const float IncumbentScore = Score(Candidates[Incumbent], MemoryWindowSeconds)
		+ AIB::TargetIncumbentBonus;
	if (BestOther != INDEX_NONE && BestOtherScore > IncumbentScore + AIB::TargetSwitchMargin)
	{
		return BestOther;
	}
	return Incumbent;
}
