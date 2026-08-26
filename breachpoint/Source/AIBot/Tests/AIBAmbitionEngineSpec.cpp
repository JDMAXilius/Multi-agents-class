#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Brain/AIBAmbitionEngine.h"
#include "Core/AIBTags.h"
#include "Core/AIBTypes.h"

/**
 * The arbitration layer, proven headless — no world, no actors: an engine object, facts
 * structs, and time as numbers. Every named utility pathology from the W-REVIEW attack
 * list gets its own pin: dithering (hysteresis), commit bypass, interrupt starvation's
 * mirror (a state-shaped cliff erasing every wounded bot's commits), the mode-objective
 * join, and the unknown-facts path.
 */
BEGIN_DEFINE_SPEC(FAIBAmbitionEngineSpec, "AIBot.Sim.AmbitionEngine",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	UAIBAmbitionEngine* Engine = nullptr;

	/** A constant-scored ambition: BaseUtility IS the score — the controlled lever. */
	FAIBAmbitionSpec Constant(FGameplayTag Tag, float Base, float Commit = 0.f) const
	{
		FAIBAmbitionSpec Spec;
		Spec.Tag = Tag;
		Spec.BaseUtility = Base;
		Spec.CommitSeconds = Commit;
		return Spec;
	}

	/** Scores Base only while the target is visible — the controlled switchable. */
	FAIBAmbitionSpec VisibleGated(FGameplayTag Tag, float Base, float Commit = 0.f) const
	{
		FAIBAmbitionSpec Spec = Constant(Tag, Base, Commit);
		FAIBConsideration& Sees = Spec.Considerations.AddDefaulted_GetRef();
		Sees.Selector = EAIBFactSelector::TargetVisible;
		Sees.SetLinearCurve(true);
		Sees.ValueWhenUnknown = 0.f;
		return Spec;
	}


	/**
	 * TestEqual has no FGameplayTag overload. Comparing GetTagName() takes the FName one,
	 * which PRINTS both tags on failure — a spec that fails without naming the ambition
	 * that won costs the next session more than it saves this one.
	 */
	bool TestTag(const TCHAR* What, FGameplayTag Actual, FGameplayTag Expected)
	{
		return TestEqual(What, Actual.GetTagName(), Expected.GetTagName());
	}

END_DEFINE_SPEC(FAIBAmbitionEngineSpec)

void FAIBAmbitionEngineSpec::Define()
{
	BeforeEach([this]()
	{
		Engine = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient);
		Engine->AddToRoot();
	});

	AfterEach([this]()
	{
		if (Engine)
		{
			Engine->RemoveFromRoot();
			Engine = nullptr;
		}
	});

	It("returns the empty tag from an empty registry — no wants, no winner, no crash", [this]()
	{
		const FAIBFacts Facts;
		TestFalse(TEXT("empty"), Engine->Rescore(Facts, 1.0).IsValid());
	});

	It("replaces on re-registration — a mode re-contributing converges", [this]()
	{
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.2f));
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.9f));
		TestEqual(TEXT("one entry"), Engine->NumAmbitions(), 1);

		const FAIBFacts Facts;
		Engine->Rescore(Facts, 1.0);
		TestEqual(TEXT("the second registration's score"), Engine->GetCurrentScore(), 0.9f);
	});

	It("multiplies considerations — one zero VETOES, whatever the rest say", [this]()
	{
		Engine->RegisterAmbition(VisibleGated(AIBTags::Ambition_Engage, 100.f));
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.2f));

		FAIBFacts Facts; // nothing visible: the 100 collapses to 0
		TestTag(TEXT("the giant is vetoed"), Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);

		Facts.bTargetVisible = true;
		TestTag(TEXT("visible flips it"), Engine->Rescore(Facts, 10.0), AIBTags::Ambition_Engage);
	});

	It("holds the incumbent against a marginal challenger — the anti-dither hysteresis", [this]()
	{
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f));
		const FAIBFacts Facts;
		Engine->Rescore(Facts, 1.0);
		TestTag(TEXT("Roam holds"), Engine->GetCurrent(), AIBTags::Ambition_Roam);

		// 1.1 < 1.0 * SwitchCostFactor(1.15): a marginal better does not flicker.
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Search, 1.1f));
		TestTag(TEXT("marginal challenger refused"), Engine->Rescore(Facts, 2.0), AIBTags::Ambition_Roam);

		// 1.3 > 1.15: a real difference switches.
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Engage, 1.3f));
		TestTag(TEXT("clear challenger accepted"), Engine->Rescore(Facts, 3.0), AIBTags::Ambition_Engage);
	});

	It("honours the commit window, then releases at its edge", [this]()
	{
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f, /*Commit=*/3.f));
		FAIBFacts Facts;
		Engine->Rescore(Facts, 10.0); // Roam wins, committed to 13.0

		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Engage, 5.0f));
		TestTag(TEXT("held inside the window"), Engine->Rescore(Facts, 12.0), AIBTags::Ambition_Roam);
		TestTag(TEXT("released after it"), Engine->Rescore(Facts, 13.1), AIBTags::Ambition_Engage);
	});

	It("voids a commit for an imminent blast — but distance in TIME matters", [this]()
	{
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f, 5.f));
		FAIBFacts Facts;
		Engine->Rescore(Facts, 10.0);
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Retreat, 5.0f));

		// A blast 4s out is beyond BlastInterruptSeconds (2.5): the commit holds.
		Facts.bIncomingBlast = true;
		Facts.BlastSecondsToDetonation = 4.0f;
		TestTag(TEXT("far blast does not void"), Engine->Rescore(Facts, 11.0), AIBTags::Ambition_Roam);

		// 1s out: void, and the stronger want takes over NOW.
		Facts.BlastSecondsToDetonation = 1.0f;
		TestTag(TEXT("imminent blast voids the commit"), Engine->Rescore(Facts, 11.5), AIBTags::Ambition_Retreat);
	});

	It("breaks a commit on the health CLIFF's edge — once, not every think thereafter", [this]()
	{
		Engine->RegisterAmbition(VisibleGated(AIBTags::Ambition_Engage, 2.0f, /*Commit=*/10.f));
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f, /*Commit=*/10.f));

		FAIBFacts Facts;
		Facts.bTargetVisible = true;
		Facts.bVitalsKnown = true;
		Facts.HealthNorm = 0.8f;
		Engine->Rescore(Facts, 10.0); // Engage, committed to 20.0

		// Crossing the cliff (0.8 -> 0.2) voids the commit; target lost the same think,
		// so Roam wins on score.
		Facts.HealthNorm = 0.2f;
		Facts.bTargetVisible = false;
		TestTag(TEXT("the crossing breaks the commit"), Engine->Rescore(Facts, 11.0), AIBTags::Ambition_Roam);

		// STAYING hurt is not a new crossing: Roam's fresh 10s commit must survive a
		// stronger challenger while health sits below the cliff — the state-shaped
		// cliff would erase every wounded bot's commits forever.
		Facts.bTargetVisible = true; // Engage would score 2.0 > Roam's 1.15
		TestTag(TEXT("sitting hurt does not re-break"), Engine->Rescore(Facts, 12.0), AIBTags::Ambition_Roam);
	});

	It("joins each mode ambition to ITS objective fact by tag — CTF stays separable", [this]()
	{
		const FGameplayTag Mode = AIBTags::Ambition_Mode;

		FAIBAmbitionSpec Urgent = Constant(Mode, 1.0f);
		FAIBConsideration& Urgency = Urgent.Considerations.AddDefaulted_GetRef();
		Urgency.Selector = EAIBFactSelector::ObjectiveUrgency;
		Urgency.SetLinearCurve(true);
		Urgency.ValueWhenUnknown = 0.f; // no matching entry -> this ambition is silent
		Engine->RegisterAmbition(Urgent);
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.3f));

		FAIBFacts Facts;
		TestTag(TEXT("no objective fact: mode ambition silent"),
			Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);

		FAIBObjectiveFact& Objective = Facts.Objectives.AddDefaulted_GetRef();
		Objective.AmbitionTag = Mode;
		Objective.Urgency = 1.0f; // the dropped flag outshouts
		TestTag(TEXT("urgency 1.0 wins"), Engine->Rescore(Facts, 10.0), Mode);
	});

	It("does not flee on unknown vitals — the default set's honest-unknown ruling", [this]()
	{
		TArray<FAIBAmbitionSpec> Defaults;
		UAIBAmbitionEngine::BuildDefaultCoreAmbitions(Defaults);
		for (const FAIBAmbitionSpec& Spec : Defaults)
		{
			Engine->RegisterAmbition(Spec);
		}
		TestEqual(TEXT("five core wants"), Engine->NumAmbitions(), 5);

		// Unknown vitals, working gun, nothing visible: a broken adapter must not read
		// as a dying bot. Roam is the honest answer, not Retreat.
		FAIBFacts Facts;
		Facts.bWeaponCanFight = true;
		TestTag(TEXT("unknown vitals roam, not rout"),
			Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);
	});

	It("wants nothing this world cannot satisfy — SeekWeapon is retired, Seek moves", [this]()
	{
		// THE BUG (25 Aug): an empty-handed bot scored SeekWeapon 1.40 on every think, its
		// branch had no weapon POI to path to, and seven bots stood still for a match.
		// Founder ruling: the concept is REMOVED, not scored down — an unsatisfiable want
		// is a trap at any utility. What is left in its place always has somewhere to go.
		TArray<FAIBAmbitionSpec> Defaults;
		UAIBAmbitionEngine::BuildDefaultCoreAmbitions(Defaults);
		for (const FAIBAmbitionSpec& Spec : Defaults)
		{
			TestFalse(TEXT("no ambition is named SeekWeapon any more"),
				Spec.Tag.ToString().Contains(TEXT("SeekWeapon")));
			Engine->RegisterAmbition(Spec);
		}
		TestEqual(TEXT("still five core wants"), Engine->NumAmbitions(), 5);

		// The exact old trap state — cannot fight, nothing visible, no mode naming a
		// destination. The old brain froze here at SeekWeapon 1.40; the floor now takes
		// it and the bot wanders, which is movement rather than a fetch quest.
		FAIBFacts Facts;
		Facts.bVitalsKnown = true;
		Facts.HealthNorm = 1.f;
		Facts.bWeaponCanFight = false;
		TestTag(TEXT("dry and blind: the floor, and the floor MOVES"),
			Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);

		// And Seek is dormant, not dead: name a destination and it outranks the floor at
		// once. That is the difference from SeekWeapon, which nothing could ever name.
		Engine->ResetArbitration();
		FAIBObjectiveFact& Somewhere = Facts.Objectives.AddDefaulted_GetRef();
		Somewhere.AmbitionTag = AIBTags::Ambition_Seek;
		Somewhere.Urgency = 1.f;
		TestTag(TEXT("a mode names a place: SEEK, above the floor"),
			Engine->Rescore(Facts, 2.0), AIBTags::Ambition_Seek);

		// ...and it never outranks a fight: the same urgency loses to a visible enemy.
		Engine->ResetArbitration();
		Facts.bWeaponCanFight = true;
		Facts.bHasTarget = true;
		Facts.bTargetVisible = true;
		Facts.DistToTargetUU = 800.f;
		TestTag(TEXT("an enemy outranks an errand"),
			Engine->Rescore(Facts, 3.0), AIBTags::Ambition_Engage);
	});

	It("releases a commit whose incumbent VETOED itself — no chasing corpses", [this]()
	{
		// W-REVIEW P2 H-2: Engage committed, target dies one think later. The old gate
		// held the zero-scored incumbent for the whole window.
		Engine->RegisterAmbition(VisibleGated(AIBTags::Ambition_Engage, 2.0f, /*Commit=*/5.f));
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.2f));

		FAIBFacts Facts;
		Facts.bTargetVisible = true;
		Engine->Rescore(Facts, 10.0); // Engage, committed to 15.0

		Facts.bTargetVisible = false; // the veto: Engage's fresh score is 0
		TestTag(TEXT("the vetoed commit releases immediately"),
			Engine->Rescore(Facts, 11.0), AIBTags::Ambition_Roam);
	});

	It("never lets the floor ambition starve a real want — Roam carries no commit", [this]()
	{
		// W-REVIEW P2 H-1: the first draft's Roam commit made every fresh spawn deaf
		// to a visible enemy for up to six seconds.
		TArray<FAIBAmbitionSpec> Defaults;
		UAIBAmbitionEngine::BuildDefaultCoreAmbitions(Defaults);
		for (const FAIBAmbitionSpec& Spec : Defaults)
		{
			if (Spec.Tag == AIBTags::Ambition_Roam)
			{
				TestEqual(TEXT("the floor never commits"), Spec.CommitSeconds, 0.f);
			}
			Engine->RegisterAmbition(Spec);
		}

		// The walked scenario: spawn (Roam wins), enemy appears half a second later.
		FAIBFacts Facts;
		Facts.bVitalsKnown = true;
		Facts.HealthNorm = 1.f;
		Facts.bWeaponCanFight = true;
		Engine->Rescore(Facts, 0.1);
		TestTag(TEXT("fresh spawn roams"), Engine->GetCurrent(), AIBTags::Ambition_Roam);

		Facts.bHasTarget = true;
		Facts.bTargetVisible = true;
		Facts.DistToTargetUU = 800.f;
		TestTag(TEXT("the first visible enemy is engaged AT ONCE"),
			Engine->Rescore(Facts, 0.5), AIBTags::Ambition_Engage);
	});

	It("dies with the body: ResetArbitration clears the commit, keeps the registry", [this]()
	{
		// W-REVIEW P2 M-1 (two passes independently): the respawned bot must not
		// resume the dead life's want, and an absolute-time CommitEnd must not
		// survive into a new clock.
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f, /*Commit=*/50.f));
		const FAIBFacts Facts;
		Engine->Rescore(Facts, 10.0);
		TestTrue(TEXT("committed"), Engine->GetCommitEndSeconds() > 10.0);

		Engine->ResetArbitration();
		TestFalse(TEXT("no winner"), Engine->GetCurrent().IsValid());
		TestTrue(TEXT("no commit"), Engine->GetCommitEndSeconds() < 0.0);
		TestEqual(TEXT("registry SURVIVES"), Engine->NumAmbitions(), 1);

		// A new life at a new clock arbitrates fresh.
		TestTag(TEXT("fresh arbitration"), Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);
	});

	It("fires the blast interrupt on its RISING EDGE only — no dither window", [this]()
	{
		// W-REVIEW P2 M5: a state-shaped blast disabled the commit for its whole 2.5s.
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f, /*Commit=*/20.f));
		FAIBFacts Facts;
		Engine->Rescore(Facts, 10.0); // committed to 30.0
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Retreat, 5.0f, /*Commit=*/20.f));

		Facts.bIncomingBlast = true;
		Facts.BlastSecondsToDetonation = 1.0f;
		// The edge: void + switch to the stronger want, which arms ITS commit.
		TestTag(TEXT("edge voids and switches"), Engine->Rescore(Facts, 11.0), AIBTags::Ambition_Retreat);

		// Still imminent next think: NOT a new edge — Retreat's commit must hold even
		// against a stronger newcomer, or the bot re-picks under the grenade.
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Engage, 9.0f));
		Facts.BlastSecondsToDetonation = 0.8f;
		TestTag(TEXT("the state does not re-fire"), Engine->Rescore(Facts, 11.2), AIBTags::Ambition_Retreat);
	});

	It("varies Engage with distance — the range consideration is NOT inert", [this]()
	{
		// W-REVIEW P2 C3: the first band sat outside the sight envelope and evaluated
		// to 1.0 forever. This pins that distance now changes the score.
		TArray<FAIBAmbitionSpec> Defaults;
		UAIBAmbitionEngine::BuildDefaultCoreAmbitions(Defaults);
		for (const FAIBAmbitionSpec& Spec : Defaults)
		{
			Engine->RegisterAmbition(Spec);
		}

		FAIBFacts Facts;
		Facts.bVitalsKnown = true;
		Facts.HealthNorm = 1.f;
		Facts.bWeaponCanFight = true;
		Facts.bHasTarget = true;
		Facts.bTargetVisible = true;

		Facts.DistToTargetUU = AIB::EngageFullAppetiteUU - 100.f;
		Engine->Rescore(Facts, 1.0);
		const float NearScore = Engine->GetCurrentScore();

		Engine->ResetArbitration();
		Facts.DistToTargetUU = AIB::EngageFadeEndUU - 1.f; // inside the envelope's edge
		Engine->Rescore(Facts, 100.0);
		const float FarScore = Engine->GetCurrentScore();

		TestTrue(TEXT("near beats far — the band lives INSIDE the envelope"), NearScore > FarScore);
	});

	It("treats an unauthored curve as identity — the pass-through contract, pinned", [this]()
	{
		// AIB1 watch-list item converted to a fact: no keys => Eval returns the
		// normalized input, so a default-constructed consideration is a linear ramp.
		FAIBConsideration Bare;
		Bare.Selector = EAIBFactSelector::AmmoNorm;
		FAIBFacts Facts;
		Facts.AmmoNorm = 0.75f;
		TestEqual(TEXT("identity pass-through"), Bare.Evaluate(Facts, nullptr), 0.75f, 0.001f);
	});

	It("separates TWO mode ambitions by their own urgencies — actual separability", [this]()
	{
		// W-REVIEW P2 M-5: the old test's title claimed this and never exercised it.
		const FGameplayTag Capture = AIBTags::Ambition_Mode;
		const FGameplayTag Defend = AIBTags::Ambition_Search; // any second distinct tag

		auto MakeMode = [this](FGameplayTag Tag)
		{
			FAIBAmbitionSpec Spec = Constant(Tag, 1.0f);
			FAIBConsideration& Urgency = Spec.Considerations.AddDefaulted_GetRef();
			Urgency.Selector = EAIBFactSelector::ObjectiveUrgency;
			Urgency.SetLinearCurve(true);
			Urgency.ValueWhenUnknown = 0.f;
			return Spec;
		};
		Engine->RegisterAmbition(MakeMode(Capture));
		Engine->RegisterAmbition(MakeMode(Defend));

		FAIBFacts Facts;
		FAIBObjectiveFact& CaptureFact = Facts.Objectives.AddDefaulted_GetRef();
		CaptureFact.AmbitionTag = Capture;
		CaptureFact.Urgency = 0.9f;
		FAIBObjectiveFact& DefendFact = Facts.Objectives.AddDefaulted_GetRef();
		DefendFact.AmbitionTag = Defend;
		DefendFact.Urgency = 0.1f;

		TestTag(TEXT("the urgent one wins"), Engine->Rescore(Facts, 1.0), Capture);
		// And a flattened builder could not produce this: the quiet one scored low.
		for (const FAIBScoredAmbition& Row : Engine->GetLastScores())
		{
			if (Row.Tag == Defend)
			{
				TestTrue(TEXT("the quiet one stayed quiet"), Row.Score < 0.2f);
			}
		}
	});

	It("keeps the full scoreboard readable — arbitration is never a black box", [this]()
	{
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f));
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Search, 0.5f));
		const FAIBFacts Facts;
		Engine->Rescore(Facts, 1.0);
		Engine->Rescore(Facts, 2.0); // second pass: Roam is now the flagged incumbent

		const TArray<FAIBScoredAmbition>& Scores = Engine->GetLastScores();
		TestEqual(TEXT("every ambition has a row"), Scores.Num(), 2);
		int32 IncumbentRows = 0;
		for (const FAIBScoredAmbition& Row : Scores)
		{
			IncumbentRows += Row.bWasIncumbent ? 1 : 0;
			if (Row.bWasIncumbent)
			{
				// RAW utility on the board — hysteresis lives only in the selection
				// comparison, so the debugger never reads an inflated number (P2 L-3).
				TestEqual(TEXT("incumbent row is raw, not x1.15"), Row.Score, 1.0f, 0.001f);
			}
		}
		TestEqual(TEXT("exactly one incumbent"), IncumbentRows, 1);
		TestTag(TEXT("runner-up identified for the instrument"),
			Engine->GetLastRunnerUp().Tag, AIBTags::Ambition_Search);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
