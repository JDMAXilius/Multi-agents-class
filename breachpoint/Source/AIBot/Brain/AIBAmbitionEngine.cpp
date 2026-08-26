#include "Brain/AIBAmbitionEngine.h"

#include "Core/AIBTags.h"

void UAIBAmbitionEngine::RegisterAmbition(const FAIBAmbitionSpec& Spec)
{
	if (!Spec.Tag.IsValid())
	{
		return; // an unnameable want cannot win, be logged, or be debugged — refuse it
	}
	// Re-registration replaces: a mode re-contributing its ambitions converges.
	for (FAIBAmbitionSpec& Existing : Ambitions)
	{
		if (Existing.Tag == Spec.Tag)
		{
			Existing = Spec;
			return;
		}
	}
	Ambitions.Add(Spec);
}

void UAIBAmbitionEngine::ClearAmbitions()
{
	Ambitions.Reset();
	CurrentTag = FGameplayTag();
	CurrentScore = 0.f;
	CommitEndSeconds = -1.0;
	LastRescoreHealthNorm = -1.f;
	LastScores.Reset();
}

bool UAIBAmbitionEngine::IsHardInterrupt(const FAIBFacts& Facts) const
{
	if (Facts.bIncomingBlast && Facts.BlastSecondsToDetonation <= BlastInterruptSeconds)
	{
		return true;
	}
	// The cliff is an EDGE, not a state: crossing below since the last rescore breaks a
	// commit once; sitting hurt does not re-break every think (that would erase commit
	// windows for any wounded bot — interrupt starvation's mirror image).
	if (Facts.bVitalsKnown && LastRescoreHealthNorm >= 0.f
		&& Facts.HealthNorm < HealthCliffNorm && LastRescoreHealthNorm >= HealthCliffNorm)
	{
		return true;
	}
	return false;
}

const FAIBObjectiveFact* UAIBAmbitionEngine::MatchObjective(const FAIBFacts& Facts, const FGameplayTag& Tag) const
{
	for (const FAIBObjectiveFact& Objective : Facts.Objectives)
	{
		if (Objective.AmbitionTag == Tag)
		{
			return &Objective;
		}
	}
	return nullptr;
}

FGameplayTag UAIBAmbitionEngine::Rescore(const FAIBFacts& Facts, double NowSeconds)
{
	LastScores.Reset();

	if (Ambitions.Num() == 0)
	{
		CurrentTag = FGameplayTag();
		CurrentScore = 0.f;
		return CurrentTag;
	}

	const bool bInterrupted = IsHardInterrupt(Facts);
	if (Facts.bVitalsKnown)
	{
		LastRescoreHealthNorm = Facts.HealthNorm;
	}

	// Score everything — introspection stays live even inside a commit window.
	const FAIBAmbitionSpec* Best = nullptr;
	float BestScore = -1.f;
	for (const FAIBAmbitionSpec& Spec : Ambitions)
	{
		const FAIBObjectiveFact* Matched = MatchObjective(Facts, Spec.Tag);

		float Score = FMath::Max(Spec.BaseUtility, 0.f);
		for (const FAIBConsideration& Consideration : Spec.Considerations)
		{
			Score *= Consideration.Evaluate(Facts, Matched);
		}

		const bool bIncumbent = (Spec.Tag == CurrentTag);
		if (bIncumbent)
		{
			Score *= SwitchCostFactor; // hysteresis: a marginal challenger does not flicker
		}

		FAIBScoredAmbition& Row = LastScores.AddDefaulted_GetRef();
		Row.Tag = Spec.Tag;
		Row.Score = Score;
		Row.bWasIncumbent = bIncumbent;

		if (Score > BestScore)
		{
			BestScore = Score;
			Best = &Spec;
		}
	}

	// The commit: inside the window, the incumbent holds — unless a hard interrupt
	// voided it. The interrupt does not PICK; scoring above already did.
	const bool bCommitted = (CommitEndSeconds > NowSeconds) && CurrentTag.IsValid() && !bInterrupted;
	if (bCommitted)
	{
		return CurrentTag;
	}

	if (Best && Best->Tag != CurrentTag)
	{
		CurrentTag = Best->Tag;
		CommitEndSeconds = NowSeconds + FMath::Max(Best->CommitSeconds, 0.f);
	}
	CurrentScore = BestScore;
	return CurrentTag;
}

void UAIBAmbitionEngine::BuildDefaultCoreAmbitions(TArray<FAIBAmbitionSpec>& OutSpecs)
{
	OutSpecs.Reset();

	// ENGAGE — someone visible, a working gun, health worth spending. The distance
	// consideration reads RAW uu against a stated band (the scale law): full appetite
	// inside 1500uu, fading to 0.3 by 3000 — never zero, a visible enemy always matters.
	{
		FAIBAmbitionSpec& Engage = OutSpecs.AddDefaulted_GetRef();
		Engage.Tag = AIBTags::Ambition_Engage;
		Engage.BaseUtility = 1.0f;
		Engage.CommitSeconds = 5.f;

		FAIBConsideration& Sees = Engage.Considerations.AddDefaulted_GetRef();
		Sees.Selector = EAIBFactSelector::TargetVisible;
		Sees.SetLinearCurve(true);
		Sees.ValueWhenUnknown = 0.f;

		FAIBConsideration& Gun = Engage.Considerations.AddDefaulted_GetRef();
		Gun.Selector = EAIBFactSelector::WeaponCanFight;
		Gun.SetLinearCurve(true);
		Gun.ValueWhenUnknown = 0.f;

		FAIBConsideration& Range = Engage.Considerations.AddDefaulted_GetRef();
		Range.Selector = EAIBFactSelector::DistToTargetUU;
		Range.InputMin = 1500.f;
		Range.InputMax = 3000.f;
		{
			FRichCurve* C = Range.Curve.GetRichCurve();
			C->Reset();
			C->AddKey(0.f, 1.f);
			C->AddKey(1.f, 0.3f);
		}
		Range.ValueWhenUnknown = 0.5f;
	}

	// RETREAT — hurt and being hurt. Health falling below ~40% ramps it; taking recent
	// damage doubles the pressure. Confidence (Phase 5) scales this further.
	{
		FAIBAmbitionSpec& Retreat = OutSpecs.AddDefaulted_GetRef();
		Retreat.Tag = AIBTags::Ambition_Retreat;
		Retreat.BaseUtility = 1.2f;
		Retreat.CommitSeconds = 3.f;

		FAIBConsideration& Hurt = Retreat.Considerations.AddDefaulted_GetRef();
		Hurt.Selector = EAIBFactSelector::HealthNorm;
		Hurt.InputMin = 0.f;
		Hurt.InputMax = 0.6f;
		Hurt.SetLinearCurve(false); // low health -> high want
		Hurt.ValueWhenUnknown = 0.f; // unknown vitals must not trigger flight

		FAIBConsideration& UnderFire = Retreat.Considerations.AddDefaulted_GetRef();
		UnderFire.Selector = EAIBFactSelector::RecentDamageTakenNorm;
		UnderFire.InputMin = 0.f;
		UnderFire.InputMax = 0.5f;
		{
			FRichCurve* C = UnderFire.Curve.GetRichCurve();
			C->Reset();
			C->AddKey(0.f, 0.25f); // merely being hurt is not yet a rout
			C->AddKey(1.f, 1.f);
		}
		UnderFire.ValueWhenUnknown = 0.25f;
	}

	// SEARCH — a fresh memory and nothing visible. Freshness IS the curve.
	{
		FAIBAmbitionSpec& Search = OutSpecs.AddDefaulted_GetRef();
		Search.Tag = AIBTags::Ambition_Search;
		Search.BaseUtility = 0.8f;
		Search.CommitSeconds = 4.f;

		FAIBConsideration& NotSeeing = Search.Considerations.AddDefaulted_GetRef();
		NotSeeing.Selector = EAIBFactSelector::TargetVisible;
		NotSeeing.SetLinearCurve(false); // visible -> 0: seeing someone ends searching
		NotSeeing.ValueWhenUnknown = 1.f;

		FAIBConsideration& Fresh = Search.Considerations.AddDefaulted_GetRef();
		Fresh.Selector = EAIBFactSelector::MemoryFreshness;
		Fresh.SetLinearCurve(true);
		Fresh.ValueWhenUnknown = 0.f; // no memory, no search — Roam owns wandering
	}

	// SEEKWEAPON — the gun cannot fight. Near-veto shape: with a working gun this
	// ambition is ~0; dry, it beats everything except a visible-enemy Engage.
	{
		FAIBAmbitionSpec& Seek = OutSpecs.AddDefaulted_GetRef();
		Seek.Tag = AIBTags::Ambition_SeekWeapon;
		Seek.BaseUtility = 1.4f;
		Seek.CommitSeconds = 4.f;

		FAIBConsideration& Dry = Seek.Considerations.AddDefaulted_GetRef();
		Dry.Selector = EAIBFactSelector::WeaponCanFight;
		Dry.SetLinearCurve(false); // can fight -> 0 want
		Dry.ValueWhenUnknown = 0.f;
	}

	// ROAM — the floor under everything: a bot with no other want walks the map.
	// No considerations: a constant, beatable by any real want.
	{
		FAIBAmbitionSpec& Roam = OutSpecs.AddDefaulted_GetRef();
		Roam.Tag = AIBTags::Ambition_Roam;
		Roam.BaseUtility = 0.2f;
		Roam.CommitSeconds = 6.f;
	}
}
