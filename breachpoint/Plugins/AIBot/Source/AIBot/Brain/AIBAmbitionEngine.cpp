#include "Brain/AIBAmbitionEngine.h"

#include "Core/AIBTags.h"
#include "Interfaces/AIBAmbitionProvider.h"

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

bool UAIBAmbitionEngine::HasAmbition(FGameplayTag Tag) const
{
	for (const FAIBAmbitionSpec& Spec : Ambitions)
	{
		if (Spec.Tag == Tag)
		{
			return true;
		}
	}
	return false;
}

void UAIBAmbitionEngine::BuildModeAmbitionSpec(const FAIBModeAmbition& Mode, FAIBAmbitionSpec& OutSpec)
{
	OutSpec = FAIBAmbitionSpec();
	OutSpec.Tag = Mode.AmbitionTag;
	OutSpec.BaseUtility = Mode.BaseUtility;
	OutSpec.CommitSeconds = 3.f;

	// The urgency IS the want: linear, and SILENT (0) when the facts carry no matching
	// objective — a mode ambition with no fact behind it must lose to everything,
	// Roam included, or the constant-that-camps defect returns.
	FAIBConsideration& Urgency = OutSpec.Considerations.AddDefaulted_GetRef();
	Urgency.Selector = EAIBFactSelector::ObjectiveUrgency;
	Urgency.SetLinearCurve(true);
	Urgency.ValueWhenUnknown = 0.f;

	// Phase 7: the claim veto, at the SAME translation site that keeps mode wants
	// honest — falling, so claimed (1) scores EXACTLY 0 and the multiplicative product
	// dies. Exactly 0 is what releases a committed loser in one rescore (the engine's
	// zero-score veto); an epsilon would hold it on a dead route for the whole window.
	// ValueWhenUnknown = 1: an unknown claim state waves through — the urgency
	// consideration above already silences the no-fact case, and a double veto would
	// hide which law fired.
	FAIBConsideration& Claimed = OutSpec.Considerations.AddDefaulted_GetRef();
	Claimed.Selector = EAIBFactSelector::ObjectiveClaimedElsewhere;
	Claimed.SetLinearCurve(false);
	Claimed.ValueWhenUnknown = 1.f;
}

void UAIBAmbitionEngine::ResetArbitration()
{
	Failures.Reset(); // strikes die with the body, like the commit clock
	CurrentTag = FGameplayTag();
	CurrentScore = 0.f;
	CommitEndSeconds = -1.0;
	LastRescoreHealthNorm = -1.f;
	bBlastWasImminent = false;
	bLastRescoreInterrupted = false;
	LastSwitchReason = EAIBSwitchReason::None;
	LastScores.Reset();
	LastRunnerUp = FAIBScoredAmbition();
}

const TCHAR* UAIBAmbitionEngine::SwitchReasonName(EAIBSwitchReason Reason)
{
	switch (Reason)
	{
	case EAIBSwitchReason::First:     return TEXT("first");
	case EAIBSwitchReason::Merit:     return TEXT("merit");
	case EAIBSwitchReason::Veto:      return TEXT("veto");
	case EAIBSwitchReason::Interrupt: return TEXT("interrupt");
	default:                          return TEXT("none");
	}
}

uint32 UAIBAmbitionEngine::FactsCrc32(const FAIBFacts& F)
{
	// Quantised to 3 dp and packed as ints: the fingerprint must survive float noise
	// between two seeded runs and still flip on any decision-relevant change.
	TArray<int32> Q;
	Q.Reserve(40);
	const auto Add = [&Q](float V) { Q.Add(FMath::RoundToInt(V * 1000.f)); };
	const auto AddB = [&Q](bool B) { Q.Add(B ? 1 : 0); };
	AddB(F.bVitalsKnown); Add(F.HealthNorm); Add(F.ShieldNorm); Add(F.AmmoNorm);
	AddB(F.bHasReserveAmmo); AddB(F.bWeaponCanFight); Q.Add(F.GrenadeCount); AddB(F.bGrounded);
	AddB(F.bHasTarget); AddB(F.bTargetVisible); AddB(F.bTargetAlive); AddB(F.bTargetFactsFromMemory);
	AddB(F.bTargetVitalsKnown); Add(F.TargetHealthNorm); Add(F.DistToTargetUU); Add(F.HeightAdvantageUU);
	AddB(F.bHasMemory); Add(F.LastKnownAgeSeconds); Add(F.MemoryFreshWindowSeconds);
	AddB(F.bIncomingBlast); Add(F.BlastSecondsToDetonation);
	Add(F.BlastCenterRelative.X); Add(F.BlastCenterRelative.Y); Add(F.BlastCenterRelative.Z); Add(F.BlastRadius);
	AddB(F.bDamageHistoryKnown); Add(F.RecentDamageTakenNorm); Add(F.RecentDamageDealtNorm);
	AddB(F.bCrowdKnown); Q.Add(F.NearbyAllies); Q.Add(F.NearbyEnemies);
	AddB(F.bConfidenceKnown); Add(F.ConfidenceNorm);
	for (const FAIBObjectiveFact& O : F.Objectives)
	{
		Q.Add(static_cast<int32>(FCrc::StrCrc32(*O.AmbitionTag.ToString())));
		Add(O.DistanceUU); Add(O.Urgency); AddB(O.bClaimedElsewhere);
	}
	return FCrc::MemCrc32(Q.GetData(), Q.Num() * sizeof(int32));
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

void UAIBAmbitionEngine::NoteAmbitionFailed(FGameplayTag Tag, double NowSeconds)
{
	if (!Tag.IsValid())
	{
		return;
	}
	FAIBFailureRecord& Record = Failures.FindOrAdd(Tag);

	// FORGET FIRST. A want that failed once a minute ago and once now is not a want that
	// failed twice in a row — without this, strikes only ever accumulate and a bot that
	// hits one bad doorway an hour eventually silences the ambition permanently.
	if (Record.LastFailureSeconds >= 0.0
		&& (NowSeconds - Record.LastFailureSeconds) > FailureForgetSeconds)
	{
		Record.Strikes = 0;
	}
	Record.LastFailureSeconds = NowSeconds;
	Record.Strikes = FMath::Min(Record.Strikes + 1, 1000);

	const float Window = FMath::Min(FailureSuppressSeconds * Record.Strikes, FailureSuppressMaxSeconds);
	// EXTEND, never shorten: two failures inside one window must not let the second
	// reset the clock to a shorter silence than the first already earned.
	Record.SuppressedUntilSeconds = FMath::Max(Record.SuppressedUntilSeconds, NowSeconds + Window);
}

bool UAIBAmbitionEngine::IsAmbitionSuppressed(FGameplayTag Tag, double NowSeconds) const
{
	const FAIBFailureRecord* Record = Failures.Find(Tag);
	return Record && NowSeconds < Record->SuppressedUntilSeconds;
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

		// THE MAKE-UP VALUE (Dave Mark's Infinite Axis Utility System compensation).
		//
		// Multiplying normalised considerations has a bias that gets worse the better you
		// model a want: four terms at a healthy 0.8 leave Engage at 0.410 of its base, while
		// a two-term want at the same quality keeps 0.640. The most carefully described
		// behaviour is the most punished, purely for being described — so authors learn to
		// add fewer considerations, which is exactly backwards.
		//
		// The compensation lifts each term toward 1 in proportion to how many terms share the
		// product. Its two endpoints are the reason it is safe here:
		//   value 0 -> 0 + (1-0)*mod*0 = 0     a veto is still a veto
		//   value 1 -> 1 + (1-1)*mod*1 = 1     a perfect term is still perfect
		// Only the middle moves. Everything this module relies on at the endpoints — a
		// suppressed want, an unknown that scores 0, Evade's silence without a blast — is
		// untouched by construction.
		const int32 NumConsiderations = Spec.Considerations.Num();
		const float ModificationFactor = NumConsiderations > 1
			? 1.f - (1.f / static_cast<float>(NumConsiderations))
			: 0.f; // a single term shares the product with nobody and needs no make-up

		float Raw = FMath::Max(Spec.BaseUtility, 0.f);
		for (const FAIBConsideration& Consideration : Spec.Considerations)
		{
			const float Value = Consideration.Evaluate(Facts, Matched);
			const float MakeUp = (1.f - Value) * ModificationFactor;
			Raw *= Value + (MakeUp * Value);
		}

		// A want whose branch just failed scores ZERO for its window — see
		// NoteAmbitionFailed. Applied HERE, to the raw score, so the scoreboard the
		// debugger reads shows the zero too: a suppressed want that silently scored
		// full utility in the instrument while losing in the selection would be the
		// same class of invisible bug this whole mechanism exists to kill.
		if (IsAmbitionSuppressed(Spec.Tag, NowSeconds))
		{
			Raw = 0.f;
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
		// `> 0` and not `!Best`: starting Best at nothing let the FIRST REGISTERED spec win
		// unconditionally on an all-zero scoreboard, so a bot would "want Engage at 0.00" and
		// enter a branch that fails on the belief it does not have. Unreachable while Roam is
		// a consideration-less floor, but the floor is a convention and this is the invariant
		// (aib-critic L3). Nothing selected is an honest answer; the tree's ungated Fallback
		// is what catches it and says so.
		if (SelectionScore > 0.f && (!Best || SelectionScore > BestSelectionScore))
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
	LastSwitchReason = EAIBSwitchReason::None;
	if (bCommitted)
	{
		CurrentScore = IncumbentRawScore; // fresh, raw — never stale across a hold
		return CurrentTag;
	}

	if (Best && Best->Tag != CurrentTag)
	{
		// AIB26: name the cause. Veto before Interrupt — a zero incumbent would have
		// switched with or without the edge, and that is the more useful reading.
		LastSwitchReason = !CurrentTag.IsValid() ? EAIBSwitchReason::First
			: IncumbentRawScore <= 0.f ? EAIBSwitchReason::Veto
			: (bInterrupted && CommitEndSeconds > NowSeconds) ? EAIBSwitchReason::Interrupt
			: EAIBSwitchReason::Merit;
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

		// PHASE 5 — confidence scales the appetite for the fight, never vetoes it: a
		// lost-feeling bot with a visible enemy still wants the fight a LITTLE (0.55),
		// which is what lets Retreat outbid it rather than Engage zeroing itself out.
		// Unknown = 1.0: a host without the damage seam keeps Phase-2 behaviour intact
		// (every pre-Phase-5 spec pin still holds).
		FAIBConsideration& Nerve = Engage.Considerations.AddDefaulted_GetRef();
		Nerve.Selector = EAIBFactSelector::ConfidenceNorm;
		{
			FRichCurve* C = Nerve.Curve.GetRichCurve();
			C->Reset();
			C->AddKey(0.f, 0.55f);
			C->AddKey(1.f, 1.f);
		}
		Nerve.ValueWhenUnknown = 1.f;

		// PHASE 12 — TWO TEAMMATES ARE ALREADY ON HIM (AIB23): the cap's SCORE-ONLY hand.
		// Saturated, Engage keeps ≈1/(1+TargetClaimCap) of itself after the make-up (the
		// constant's comment shows the arithmetic) — enough for a mode want or a fresh Roam
		// to outbid it, never a veto (F9: a bot denied a target moves, it does not idle;
		// with nothing else to want it engages at a third, which is the audit's ruling).
		FAIBConsideration& Crowd = Engage.Considerations.AddDefaulted_GetRef();
		Crowd.Selector = EAIBFactSelector::TargetClaimSaturated;
		{
			FRichCurve* C = Crowd.Curve.GetRichCurve();
			C->Reset();
			C->AddKey(0.f, 1.f);
			C->AddKey(1.f, AIB::EngageSaturatedWant);
		}
		Crowd.ValueWhenUnknown = 1.f;
	}

	// RETREAT — hurt and being hurt. Damage history has no source until Phase 3, so
	// its consideration runs on ValueWhenUnknown — an honest unknown, not a dead zero
	// hiding behind a numeric coincidence (W-REVIEW P2 M6).
	{
		FAIBAmbitionSpec& Retreat = OutSpecs.AddDefaulted_GetRef();
		Retreat.Tag = AIBTags::Ambition_Retreat;
		// 1.35, up from 1.2 (founder, 28 Aug — "make them retreat more"). MEASURED first:
		// one match chose Retreat 12 times against Engage's 154, so the defend behaviour was
		// correct and almost never seen — about one engagement in thirty.
		Retreat.BaseUtility = 1.35f;
		Retreat.CommitSeconds = 3.f;

		// VITALITY, not health. In a shielded game the moment that matters is the
		// SHIELD BREAK, and this consideration used to be blind to it: a body at full
		// health with a broken shield is one burst from death and scored 0.0 here, so
		// Retreat could not outbid anything. (The host's low-health research note carries the
		// Halo reading; it is not named here — law 1 keeps this module free of host symbols.)
		//
		// Why one combined selector rather than a second shield consideration: scores are
		// MULTIPLICATIVE. A separate term can only ever pull a want DOWN, so it could not
		// have raised Retreat at full health — and its full-shield value would have taxed
		// every health-driven retreat in a shieldless game. min() is the shape that fits:
		// the more depleted of the two layers is the one you are dying through.
		//
		// Shieldless play is byte-identical to before, by construction: ShieldNorm is 1
		// when a mode has no shields (MaxShield == 0, which is TODAY — vitals are paused
		// per the 13 Aug call), so min() collapses to HealthNorm and every existing pin
		// still holds. This arms itself the day shields come back on.
		FAIBConsideration& Hurt = Retreat.Considerations.AddDefaulted_GetRef();
		Hurt.Selector = EAIBFactSelector::VitalityNorm;
		Hurt.InputMin = 0.f;
		// 0.8, up from 0.6: the band is WHEN a bot starts caring, and at 0.6 it barely cared
		// until it was nearly dead. At half vitality the want was 0.167 of full — noise
		// against Engage — so bots fought on through wounds that should have moved them.
		// At 0.8 that same half-vitality bot scores 0.375, and a bot at 0.6 (which used to
		// score exactly ZERO) now registers at all.
		Hurt.InputMax = 0.8f;
		Hurt.SetLinearCurve(false); // low health -> high want
		Hurt.ValueWhenUnknown = 0.f; // unknown vitals must not trigger flight

		FAIBConsideration& UnderFire = Retreat.Considerations.AddDefaulted_GetRef();
		UnderFire.Selector = EAIBFactSelector::RecentDamageTakenNorm;
		UnderFire.InputMin = 0.f;
		UnderFire.InputMax = 0.5f;
		{
			FRichCurve* C = UnderFire.Curve.GetRichCurve();
			C->Reset();
			// 0.5 at the floor, up from 0.35. This term exists so that being hurt LONG AGO is
			// not a rout, and that still holds — but 0.35 also throttled the legitimate case
			// of a wounded bot taking fresh fire down to a third of its want. Half is still a
			// clear discount on a quiet moment while letting real damage read.
			C->AddKey(0.f, 0.5f);
			C->AddKey(1.f, 1.f);
		}
		UnderFire.ValueWhenUnknown = 0.5f;

		// PHASE 5 — the mirror of Engage's nerve: high confidence SUPPRESSES the urge
		// to leave (a winning bot presses through the same wounds a losing one flees),
		// low confidence leaves Retreat's full score standing. Unknown = 1.0 keeps
		// every pre-Phase-5 pin intact on hosts without the damage seam.
		FAIBConsideration& Nerve = Retreat.Considerations.AddDefaulted_GetRef();
		Nerve.Selector = EAIBFactSelector::ConfidenceNorm;
		{
			FRichCurve* C = Nerve.Curve.GetRichCurve();
			C->Reset();
			C->AddKey(0.f, 1.f);
			C->AddKey(1.f, 0.45f);
		}
		Nerve.ValueWhenUnknown = 1.f;
	}

	// EVADE — a grenade is about to go off and this bot has READ it. Its own want, and it
	// has to be, for a reason the ShieldNorm change taught the hard way: considerations
	// MULTIPLY. A blast term bolted onto Retreat could never make a HEALTHY bot dodge,
	// because Retreat's Hurt is exactly 0 above 0.8 vitality and 0 x anything is 0. The bot
	// winning a fight at full health is precisely the bot that needs to move.
	//
	// This is the most legible behaviour a Halo bot has — the scatter — and until now the
	// whole chain that feeds it (perceivability trace, fuse-noise draw, the CanEvadeBlast
	// capability gate, the reaction clock, the facts) terminated in a fact NOTHING read.
	// A grenade at a bot's feet was answered with the rest of its strafe leg.
	{
		FAIBAmbitionSpec& Evade = OutSpecs.AddDefaulted_GetRef();
		Evade.Tag = AIBTags::Ambition_Evade;
		// Above every other want by design. A dodge that loses to a fight is not a dodge, and
		// the window is ~2s — there is no time to be outbid and reconsider.
		Evade.BaseUtility = 4.f;
		// SHORT, and shorter than the fuse. The commit exists so the bot does not abandon the
		// dodge halfway; it must not outlive the blast, or a bot spends two seconds running
		// from a crater.
		Evade.CommitSeconds = 1.5f;

		// THE FUSE, falling. Full want the instant it is about to land, fading to nothing at
		// the edge of the window the interrupt already uses (BlastInterruptSeconds).
		FAIBConsideration& Fuse = Evade.Considerations.AddDefaulted_GetRef();
		Fuse.Selector = EAIBFactSelector::BlastSecondsToDetonation;
		Fuse.InputMin = 0.f;
		// The DEFAULT interrupt window, not the live member: this builder is static, and the
		// two must agree anyway — the want should fade out exactly where the commit-breaking
		// edge stops firing. Reading the CDO keeps them one number even if it is retuned.
		Fuse.InputMax = GetDefault<UAIBAmbitionEngine>()->BlastInterruptSeconds;
		{
			FRichCurve* C = Fuse.Curve.GetRichCurve();
			C->Reset();
			C->AddKey(0.f, 1.f);   // going off now
			C->AddKey(1.f, 0.2f);  // at the window's edge: worth starting to move
		}
		// ZERO when unknown, and this is the whole safety of the want. No blast, no seam, or
		// a host that never publishes one = Evade scores 0.0 and is invisible. Every existing
		// tier, spec and measurement is untouched on a build without grenades.
		Fuse.ValueWhenUnknown = 0.f;
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

	// SEEKWEAPON — the gun cannot fight AND somewhere to fix that is KNOWN. Near-veto
	// shape: with a working gun this ambition is ~0; dry with a known weapon source it
	// beats everything except a visible-enemy Engage. Dry with NO known source it scores
	// BELOW Roam — deliberately (W-REVIEW P3 H2): until a provider supplies a weapon
	// objective (Phase 6), a want the executor cannot serve must not win arbitration, or
	// every bot that empties its loadout becomes a statue seeking a weapon nobody can
	// name. Roaming keeps it moving — which is also how it stumbles onto pickups.
	{
		FAIBAmbitionSpec& Seek = OutSpecs.AddDefaulted_GetRef();
		Seek.Tag = AIBTags::Ambition_Seek;
		Seek.BaseUtility = 1.4f;
		Seek.CommitSeconds = 4.f;

		FAIBConsideration& Dry = Seek.Considerations.AddDefaulted_GetRef();
		Dry.Selector = EAIBFactSelector::WeaponCanFight;
		Dry.SetLinearCurve(false); // can fight -> 0 want
		Dry.ValueWhenUnknown = 0.f;

		FAIBConsideration& KnownSource = Seek.Considerations.AddDefaulted_GetRef();
		KnownSource.Selector = EAIBFactSelector::ObjectiveUrgency;
		KnownSource.SetLinearCurve(true);
		// ZERO, not a small number. This used to be 0.1 with the comment "1.4 x 0.1 = 0.14 <
		// Roam's 0.2 floor" — dormancy as a NUMERIC COINCIDENCE, tuned against another want's
		// base. The make-up value broke it immediately (0.1 across two terms lifts to 0.405,
		// and 1.4 x 0.405^2 = 0.23 beats the floor), and a bot wanted to Seek with nowhere to
		// go — the exact SeekWeapon trap this ambition was rewritten to escape.
		//
		// An unsatisfiable want must be VETOED, not merely outbid: nothing named a place, so
		// there is nothing to seek, and 0 says that at any base and under any compensation.
		KnownSource.ValueWhenUnknown = 0.f;
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
