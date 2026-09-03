#include "Brain/AIBConsideration.h"

namespace
{
	/** UNSET means the fact is unknowable right now — not zero, not full. */
	TOptional<float> SelectFact(EAIBFactSelector Selector, const FAIBFacts& Facts,
		const FAIBObjectiveFact* MatchedObjective)
	{
		switch (Selector)
		{
		case EAIBFactSelector::HealthNorm:
			return Facts.bVitalsKnown ? TOptional<float>(Facts.HealthNorm) : TOptional<float>();
		case EAIBFactSelector::VitalityNorm:
			return Facts.bVitalsKnown
				? TOptional<float>(FMath::Min(Facts.HealthNorm, Facts.ShieldNorm))
				: TOptional<float>();
		case EAIBFactSelector::AmmoNorm:
			return Facts.AmmoNorm;
		case EAIBFactSelector::GrenadeCount:
			return static_cast<float>(Facts.GrenadeCount);
		case EAIBFactSelector::WeaponCanFight:
			return Facts.bWeaponCanFight ? 1.f : 0.f;
		case EAIBFactSelector::HasReserveAmmo:
			return Facts.bHasReserveAmmo ? 1.f : 0.f;
		case EAIBFactSelector::TargetVisible:
			// AIB26 F8-5: a young flank latch keeps the BELIEF in the fight (the flank
			// broke LOS on purpose). Search's falling copy of this term reads it too.
			return (Facts.bTargetVisible || Facts.bFlankHolding) ? 1.f : 0.f;
		case EAIBFactSelector::TargetFactsFromMemory:
			return Facts.bTargetFactsFromMemory ? 1.f : 0.f;
		case EAIBFactSelector::TargetHealthNorm:
			return Facts.bTargetVitalsKnown ? TOptional<float>(Facts.TargetHealthNorm) : TOptional<float>();
		case EAIBFactSelector::DistToTargetUU:
			return Facts.DistToTargetUU >= 0.f ? TOptional<float>(Facts.DistToTargetUU) : TOptional<float>();
		case EAIBFactSelector::HeightAdvantageUU:
			// 0 is a legal value ("exactly level"), so absence needs the target gate,
			// not a sentinel (W-REVIEW P2 M4/L1).
			return Facts.bHasTarget ? TOptional<float>(Facts.HeightAdvantageUU) : TOptional<float>();
		case EAIBFactSelector::MemoryFreshness:
			if (!Facts.bHasMemory || Facts.MemoryFreshWindowSeconds <= 0.f)
			{
				return TOptional<float>();
			}
			return FMath::Clamp(1.f - Facts.LastKnownAgeSeconds / Facts.MemoryFreshWindowSeconds, 0.f, 1.f);
		case EAIBFactSelector::BlastSecondsToDetonation:
			return Facts.bIncomingBlast ? TOptional<float>(Facts.BlastSecondsToDetonation) : TOptional<float>();
		case EAIBFactSelector::RecentDamageTakenNorm:
			return Facts.bDamageHistoryKnown ? TOptional<float>(Facts.RecentDamageTakenNorm) : TOptional<float>();
		case EAIBFactSelector::RecentDamageDealtNorm:
			return Facts.bDamageHistoryKnown ? TOptional<float>(Facts.RecentDamageDealtNorm) : TOptional<float>();
		// Crowd facts are honest only when the builder MARKED them known: the flag fix
		// that closed the confident-zero hole in the confidence model left these three
		// selectors reading an unwritten 0 as "I am certainly alone" (W-REVIEW P4+5 C5).
		case EAIBFactSelector::NearbyAllies:
			return Facts.bCrowdKnown ? TOptional<float>(static_cast<float>(Facts.NearbyAllies)) : TOptional<float>();
		case EAIBFactSelector::NearbyEnemies:
			return Facts.bCrowdKnown ? TOptional<float>(static_cast<float>(Facts.NearbyEnemies)) : TOptional<float>();
		case EAIBFactSelector::Outnumbered:
			return Facts.bCrowdKnown
				? TOptional<float>(static_cast<float>(Facts.NearbyEnemies - Facts.NearbyAllies)) : TOptional<float>();
		case EAIBFactSelector::ObjectiveUrgency:
			return MatchedObjective ? TOptional<float>(MatchedObjective->Urgency) : TOptional<float>();
		case EAIBFactSelector::ObjectiveDistanceUU:
			return (MatchedObjective && MatchedObjective->DistanceUU >= 0.f)
				? TOptional<float>(MatchedObjective->DistanceUU) : TOptional<float>();
		case EAIBFactSelector::ConfidenceNorm:
			return Facts.bConfidenceKnown ? TOptional<float>(Facts.ConfidenceNorm) : TOptional<float>();
		case EAIBFactSelector::ObjectiveClaimedElsewhere:
			return MatchedObjective
				? TOptional<float>(MatchedObjective->bClaimedElsewhere ? 1.f : 0.f) : TOptional<float>();
		case EAIBFactSelector::TargetClaimSaturated:
			return Facts.bHasTarget ? TOptional<float>(Facts.bTargetClaimSaturated ? 1.f : 0.f) : TOptional<float>();
		}
		return TOptional<float>();
	}
}

float FAIBConsideration::Evaluate(const FAIBFacts& Facts, const FAIBObjectiveFact* MatchedObjective) const
{
	const TOptional<float> Raw = SelectFact(Selector, Facts, MatchedObjective);
	float Out;
	if (!Raw.IsSet())
	{
		Out = FMath::Clamp(ValueWhenUnknown, 0.f, 1.f);
	}
	else
	{
		// Raw range -> 0..1, stated by the author, then shaped. A degenerate range is
		// authored error: score indifferent rather than dividing by zero.
		const float Span = InputMax - InputMin;
		const float Normalized = (FMath::Abs(Span) > UE_KINDA_SMALL_NUMBER)
			? FMath::Clamp((Raw.GetValue() - InputMin) / Span, 0.f, 1.f)
			: 0.5f;
		Out = FMath::Clamp(Curve.GetRichCurveConst()->Eval(Normalized, /*default=*/Normalized), 0.f, 1.f);
	}

	// Weight as exponent: 0 disables (score 1 contributes nothing to a product).
	return FMath::Pow(Out, FMath::Max(Weight, 0.f));
}

void FAIBConsideration::SetLinearCurve(bool bRising)
{
	FRichCurve* RichCurve = Curve.GetRichCurve();
	RichCurve->Reset();
	RichCurve->AddKey(0.f, bRising ? 0.f : 1.f);
	RichCurve->AddKey(1.f, bRising ? 1.f : 0.f);
}
