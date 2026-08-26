#include "Brain/AIBAmbitionEngine.h"

#include "Core/AIBTags.h"

void UAIBAmbitionEngine::RegisterAmbition(const FAIBAmbitionSpec& Spec)
{
	if (!Spec.Tag.IsValid())
	{
		return; // an unnameable want cannot win, be logged, or be debugged — refuse it
	}
	// Re-registration replaces IN PLACE: a mode re-contributing converges, tie-break
	// order stays stable, and the commit clock is deliberately untouched (the recipe
	// swaps, the window does not — W-REVIEW P2 L-2, documented behaviour).
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
	ResetArbitration();
}

void UAIBAmbitionEngine::ResetArbitration()
{
	CurrentTag = FGameplayTag();
	CurrentScore = 0.f;
	CommitEndSeconds = -1.0;
	LastRescoreHealthNorm = -1.f;
	bBlastWasImminent = false;
	bLastRescoreInterrupted = false;
	LastScores.Reset();
	LastRunnerUp = FAIBScoredAmbition();
}

bool UAIBAmbitionEngine::IsHardInterrupt(const FAIBFacts& Facts) const
{
	// BOTH interrupts are EDGES. The blast: only the think where it BECOMES imminent
	// fires — a state-shaped blast disabled the commit for its whole 2.5s window and
	// the bot re-picked at think rate under a grenade (W-REVIEW P2 M5).
	const bool bBlastImminent = Facts.bIncomingBlast
		&& Facts.BlastSecondsToDetonation <= BlastInterruptSeconds;
	if (bBlastImminent && !bBlastWasImminent)
	{
		return true;
	}
	// The cliff: crossing below since the last rescore breaks a commit once; sitting
	// hurt does not re-break every think.
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
	LastRunnerUp = FAIBScoredAmbition();

	if (Ambitions.Num() == 0)
	{
		CurrentTag = FGameplayTag();
		CurrentScore = 0.f;
		return CurrentTag;
	}

	const bool bInterrupted = IsHardInterrupt(Facts);
	bLastRescoreInterrupted = bInterrupted;

	// Edge baselines update AFTER the edge test, so each edge fires exactly once.
	bBlastWasImminent = Facts.bIncomingBlast
		&& Facts.BlastSecondsToDetonation <= BlastInterruptSeconds;
	if (Facts.bVitalsKnown)
	{
		LastRescoreHealthNorm = Facts.HealthNorm;
	}

	// Score everything — introspection stays live even inside a commit window. The
	// scoreboard records RAW utility; hysteresis is applied only to the selection
	// comparison, so the debugger never reads an inflated number (W-REVIEW P2 L-3).
	const FAIBAmbitionSpec* Best = nullptr;
	float BestSelectionScore = 0.f;
	float BestRawScore = 0.f;
	float IncumbentRawScore = -1.f;
	for (const FAIBAmbitionSpec& Spec : Ambitions)
	{
		const FAIBObjectiveFact* Matched = MatchObjective(Facts, Spec.Tag);

		float Raw = FMath::Max(Spec.BaseUtility, 0.f);
		for (const FAIBConsideration& Consideration : Spec.Considerations)
		{
			Raw *= Consideration.Evaluate(Facts, Matched);
		}

		const bool bIncumbent = (Spec.Tag == CurrentTag);
		if (bIncumbent)
		{
			IncumbentRawScore = Raw;
		}

		FAIBScoredAmbition& Row = LastScores.AddDefaulted_GetRef();
		Row.Tag = Spec.Tag;
		Row.Score = Raw;
		Row.bWasIncumbent = bIncumbent;

		const float SelectionScore = bIncumbent ? Raw * SwitchCostFactor : Raw;
		if (!Best || SelectionScore > BestSelectionScore)
		{
			BestSelectionScore = SelectionScore;
			BestRawScore = Raw;
			Best = &Spec;
		}
	}

	// Runner-up (by raw score, excluding the winner) — the instrument's context row.
	for (const FAIBScoredAmbition& Row : LastScores)
	{
		if (Best && Row.Tag != Best->Tag && Row.Score >= LastRunnerUp.Score)
		{
			LastRunnerUp = Row;
		}
	}

	// The commit holds only while ALL THREE are true: window live, no interrupt, and
	// the incumbent has not VETOED ITSELF — a fresh score of zero is the ambition
	// declaring itself impossible, and a commit must not hold a bot to chasing a
	// corpse (W-REVIEW P2 H-2).
	const bool bCommitted = (CommitEndSeconds > NowSeconds) && CurrentTag.IsValid()
		&& !bInterrupted && IncumbentRawScore > 0.f;
	if (bCommitted)
	{
		CurrentScore = IncumbentRawScore; // fresh, raw — never stale across a hold
		return CurrentTag;
	}

	if (Best && Best->Tag != CurrentTag)
	{
		CurrentTag = Best->Tag;
		// One-shot commit ON ENTRY — and only for an ambition that asked for one. The
		// floor ambition (no considerations, nothing to dither against) carries zero
		// commit precisely so it can never starve a real want (W-REVIEW P2 H-1).
		CommitEndSeconds = NowSeconds + FMath::Max(Best->CommitSeconds, 0.f);
	}
	CurrentScore = BestRawScore;
	return CurrentTag;
}

void UAIBAmbitionEngine::BuildDefaultCoreAmbitions(TArray<FAIBAmbitionSpec>& OutSpecs)
{
	OutSpecs.Reset();

	// ENGAGE — someone visible, a working gun. The range band is NAMED module
	// constants sourced to the default sight envelope (AIB::EngageFullAppetiteUU /
	// EngageFadeEndUU) — the first draft's band sat entirely OUTSIDE the envelope and
	// evaluated to 1.0 forever (W-REVIEW P2 C3: an inert, authored, documented input).
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
		Range.InputMin = AIB::EngageFullAppetiteUU;
		Range.InputMax = AIB::EngageFadeEndUU;
		{
			FRichCurve* C = Range.Curve.GetRichCurve();
			C->Reset();
			C->AddKey(0.f, 1.f);
			C->AddKey(1.f, 0.5f); // never zero: a visible enemy always matters
		}
		Range.ValueWhenUnknown = 0.5f;
	}

	// RETREAT — hurt and being hurt. Damage history has no source until Phase 3, so
	// its consideration runs on ValueWhenUnknown — an honest unknown, not a dead zero
	// hiding behind a numeric coincidence (W-REVIEW P2 M6).
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
			C->AddKey(0.f, 0.35f); // merely being hurt is not yet a rout
			C->AddKey(1.f, 1.f);
		}
		UnderFire.ValueWhenUnknown = 0.35f;
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

	// SEEK — "I have somewhere to be." The deliberate-movement want: toward the target
	// belief when there is one, otherwise toward a point worth being at.
	//
	// IT REPLACES SEEKWEAPON (founder ruling, 25 Aug). This game has no weapon pickups
	// and no plan for them, so an ambition about fetching a gun could never be satisfied
	// — and an unsatisfiable want is a trap at ANY utility, not just at 1.40: the day a
	// consideration nudges it back to the top, the bot strands again exactly as seven of
	// them did. Scoring around it was the wrong fix; removing the concept is the right
	// one. The host's own working brain models Fight / Survive / Roam and never modelled
	// seeking a weapon, which is that game being correct about its own world.
	//
	// SATISFIABLE BY CONSTRUCTION, which is the property that matters: its branch always
	// has somewhere to walk (belief -> POI -> a random reachable point), so selecting it
	// can never mean standing still. Urgency is the mode's dial (Phase 6, via
	// IAIBAmbitionProvider, matched on this tag). With no mode registered the objective
	// fact is UNKNOWN and the honest unknown is "worth moving, nothing urgent" — 0.6,
	// which puts Seek (0.30) above the Roam floor (0.20) and well below Search (0.80).
	// A mode that says urgency 0 hands the bot straight back to Roam.
	{
		FAIBAmbitionSpec& Seek = OutSpecs.AddDefaulted_GetRef();
		Seek.Tag = AIBTags::Ambition_Seek;
		Seek.BaseUtility = 0.5f;
		Seek.CommitSeconds = 3.f;

		FAIBConsideration& Urgency = Seek.Considerations.AddDefaulted_GetRef();
		Urgency.Selector = EAIBFactSelector::ObjectiveUrgency;
		Urgency.SetLinearCurve(true); // the mode says "nothing here" -> 0 want
		Urgency.ValueWhenUnknown = 0.6f;
	}

	// ROAM — the floor under everything, and it NEVER COMMITS: a fresh bot's Roam win
	// must not make it deaf to the first enemy it sees. The first draft gave the floor
	// the LONGEST window of the five, and every spawn began with up to six seconds of
	// walking past visible enemies (W-REVIEW P2 H-1, the commit-starvation walk).
	{
		FAIBAmbitionSpec& Roam = OutSpecs.AddDefaulted_GetRef();
		Roam.Tag = AIBTags::Ambition_Roam;
		Roam.BaseUtility = 0.2f;
		Roam.CommitSeconds = 0.f;
	}
}
