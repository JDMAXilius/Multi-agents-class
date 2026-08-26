#include "Brain/AIBConfidenceModel.h"

void FAIBDamageLedger::NoteTaken(float FractionOfMaxHealth, double NowSeconds)
{
	if (FractionOfMaxHealth <= 0.f || FMath::IsNaN(FractionOfMaxHealth))
	{
		return; // heals and garbage are not damage
	}
	Taken = Decayed(Taken, TakenStamp, NowSeconds) + FractionOfMaxHealth;
	TakenStamp = NowSeconds;
}

void FAIBDamageLedger::NoteDealt(float FractionOfMaxHealth, double NowSeconds)
{
	if (FractionOfMaxHealth <= 0.f || FMath::IsNaN(FractionOfMaxHealth))
	{
		return;
	}
	Dealt = Decayed(Dealt, DealtStamp, NowSeconds) + FractionOfMaxHealth;
	DealtStamp = NowSeconds;
}

float FAIBDamageLedger::TakenNorm(double NowSeconds) const
{
	return Decayed(Taken, TakenStamp, NowSeconds);
}

float FAIBDamageLedger::DealtNorm(double NowSeconds) const
{
	return Decayed(Dealt, DealtStamp, NowSeconds);
}

void FAIBDamageLedger::Reset()
{
	Taken = 0.f;
	Dealt = 0.f;
	TakenStamp = 0.0;
	DealtStamp = 0.0;
}

float FAIBDamageLedger::Decayed(float Value, double StampedAt, double Now) const
{
	if (Value <= 0.f || Now <= StampedAt)
	{
		return Value;
	}
	// Exponential half-life: lazy decay at read time, so noting and reading are both
	// O(1) and time only ever flows through parameters (worldless law).
	const double HalfLives = (Now - StampedAt) / static_cast<double>(HalfLifeSeconds);
	return Value * static_cast<float>(FMath::Pow(0.5, HalfLives));
}

// -- the confidence model ------------------------------------------------------------

float FAIBConfidenceModel::MisjudgeAmplitude(EAIBCompetence Level)
{
	switch (Level)
	{
	case EAIBCompetence::Novice:  return 0.30f;
	case EAIBCompetence::Skilled: return 0.10f;
	case EAIBCompetence::Expert:  return 0.04f;
	case EAIBCompetence::Trained:
	default:                      return 0.18f; // out-of-range degrades to average
	}
}

float FAIBConfidenceModel::ReJudgeSeconds(EAIBCompetence Level)
{
	// Better judges also RE-judge faster — a Novice holds a wrong read of the fight
	// for two full seconds, which is the "why is it still pushing" moment a human
	// recognises from real lobbies.
	switch (Level)
	{
	case EAIBCompetence::Novice:  return 2.0f;
	case EAIBCompetence::Skilled: return 1.0f;
	case EAIBCompetence::Expert:  return 0.7f;
	case EAIBCompetence::Trained:
	default:                      return 1.4f;
	}
}

float FAIBConfidenceModel::Assess(const FAIBFacts& Facts)
{
	// A human's read of a fight, from facts a human has. Weights sum so the extremes
	// are reachable but only by everything agreeing; each input names its share.
	float Assess = 0.5f;

	// Own health: +/-0.25 across the bar. Unknown vitals contribute NOTHING — a broken
	// adapter must not read as wounded (the unknown-is-a-state law).
	if (Facts.bVitalsKnown)
	{
		Assess += (Facts.HealthNorm - 0.5f) * 0.5f;
	}

	// Momentum: +/-0.25 on the traded-damage balance. Only when a source exists.
	if (Facts.bDamageHistoryKnown)
	{
		Assess += FMath::Clamp(Facts.RecentDamageDealtNorm - Facts.RecentDamageTakenNorm, -1.f, 1.f) * 0.25f;
	}

	// The gun: a hand that cannot fight caps the read down harder than a working one
	// lifts it — being armed is expected, being helpless is alarming.
	Assess += Facts.bWeaponCanFight ? 0.10f : -0.20f;

	// Being visibly outnumbered — only when a bounded crowd read EXISTS (W-AUDIT P6:
	// without the flag this term read a dead zero as "confidently alone").
	if (Facts.bCrowdKnown && Facts.NearbyEnemies > 1)
	{
		Assess -= 0.10f;
	}

	return FMath::Clamp(Assess, 0.f, 1.f);
}

float FAIBConfidenceModel::Step(FAIBConfidenceState& State, const FAIBFacts& Facts,
	EAIBCompetence Level, FRandomStream& Rng, double NowSeconds)
{
	// The misjudge is HELD between draws — consistent wrongness reads as a bad call,
	// per-tick noise reads as a broken needle. First step draws immediately.
	if (NowSeconds >= State.NextJudgeAtSeconds)
	{
		const float Amplitude = MisjudgeAmplitude(Level);
		State.MisjudgeOffset = Rng.FRandRange(-Amplitude, Amplitude);
		State.NextJudgeAtSeconds = NowSeconds + ReJudgeSeconds(Level);
	}
	return FMath::Clamp(Assess(Facts) + State.MisjudgeOffset, 0.f, 1.f);
}
