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
		TestEqual(TEXT("the giant is vetoed"), Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);

		Facts.bTargetVisible = true;
		TestEqual(TEXT("visible flips it"), Engine->Rescore(Facts, 10.0), AIBTags::Ambition_Engage);
	});

	It("holds the incumbent against a marginal challenger — the anti-dither hysteresis", [this]()
	{
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f));
		const FAIBFacts Facts;
		Engine->Rescore(Facts, 1.0);
		TestEqual(TEXT("Roam holds"), Engine->GetCurrent(), AIBTags::Ambition_Roam);

		// 1.1 < 1.0 * SwitchCostFactor(1.15): a marginal better does not flicker.
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Search, 1.1f));
		TestEqual(TEXT("marginal challenger refused"), Engine->Rescore(Facts, 2.0), AIBTags::Ambition_Roam);

		// 1.3 > 1.15: a real difference switches.
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Engage, 1.3f));
		TestEqual(TEXT("clear challenger accepted"), Engine->Rescore(Facts, 3.0), AIBTags::Ambition_Engage);
	});

	It("honours the commit window, then releases at its edge", [this]()
	{
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f, /*Commit=*/3.f));
		FAIBFacts Facts;
		Engine->Rescore(Facts, 10.0); // Roam wins, committed to 13.0

		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Engage, 5.0f));
		TestEqual(TEXT("held inside the window"), Engine->Rescore(Facts, 12.0), AIBTags::Ambition_Roam);
		TestEqual(TEXT("released after it"), Engine->Rescore(Facts, 13.1), AIBTags::Ambition_Engage);
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
		TestEqual(TEXT("far blast does not void"), Engine->Rescore(Facts, 11.0), AIBTags::Ambition_Roam);

		// 1s out: void, and the stronger want takes over NOW.
		Facts.BlastSecondsToDetonation = 1.0f;
		TestEqual(TEXT("imminent blast voids the commit"), Engine->Rescore(Facts, 11.5), AIBTags::Ambition_Retreat);
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
		TestEqual(TEXT("the crossing breaks the commit"), Engine->Rescore(Facts, 11.0), AIBTags::Ambition_Roam);

		// STAYING hurt is not a new crossing: Roam's fresh 10s commit must survive a
		// stronger challenger while health sits below the cliff — the state-shaped
		// cliff would erase every wounded bot's commits forever.
		Facts.bTargetVisible = true; // Engage would score 2.0 > Roam's 1.15
		TestEqual(TEXT("sitting hurt does not re-break"), Engine->Rescore(Facts, 12.0), AIBTags::Ambition_Roam);
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
		TestEqual(TEXT("no objective fact: mode ambition silent"),
			Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);

		FAIBObjectiveFact& Objective = Facts.Objectives.AddDefaulted_GetRef();
		Objective.AmbitionTag = Mode;
		Objective.Urgency = 1.0f; // the dropped flag outshouts
		TestEqual(TEXT("urgency 1.0 wins"), Engine->Rescore(Facts, 10.0), Mode);
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
		TestEqual(TEXT("unknown vitals roam, not rout"),
			Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);
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
		}
		TestEqual(TEXT("exactly one incumbent"), IncumbentRows, 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
