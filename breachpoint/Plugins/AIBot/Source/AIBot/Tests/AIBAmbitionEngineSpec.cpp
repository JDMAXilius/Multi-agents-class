#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Brain/AIBAmbitionEngine.h"
#include "Core/AIBFactsBuilder.h"
#include "Core/AIBTags.h"
#include "Core/AIBTypes.h"
#include "Execution/AIBStateTreeTasks.h"
#include "Interfaces/AIBAmbitionProvider.h"
#include "NativeGameplayTags.h"
#include <limits>

/** A HOST-SHAPED child tag, registered by the spec itself: the whole point of the Mode
 *  gate is that a game's own `AIBot.Ambition.Mode.<X>` — a tag this module never names —
 *  matches the hierarchy gate. Proving it with the parent tag would prove nothing. */
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AIBSpec_Mode_Child, "AIBot.Ambition.Mode.SpecHold");

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
	/** Retreat's raw score from the last Rescore — 0 if it did not score at all. */
	float RetreatScore() const
	{
		for (const FAIBScoredAmbition& Row : Engine->GetLastScores())
		{
			if (Row.Tag == AIBTags::Ambition_Retreat)
			{
				return Row.Score;
			}
		}
		return 0.f;
	}

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

	It("breaks contact on a SHIELD break at full health — the Halo rhythm", [this]()
	{
		// THE GAP (28 Aug): Retreat's
		// danger input was HealthNorm alone. In a shielded game a bot at 100% health with a
		// broken shield is one burst from death, and it scored 0.0 on that input — so it
		// kept pushing the fight that had just stripped it. Halo's low-health loop is
		// shield-break -> break contact -> recharge -> re-engage.
		TArray<FAIBAmbitionSpec> Defaults;
		UAIBAmbitionEngine::BuildDefaultCoreAmbitions(Defaults);
		for (const FAIBAmbitionSpec& Spec : Defaults)
		{
			Engine->RegisterAmbition(Spec);
		}

		// Under fire, because a shield does not break in silence: UnderFire gates Retreat
		// at 0.35 when nothing has shot you lately ("merely being hurt is not yet a rout"),
		// and a spec that left damage unset would be testing a state that cannot occur.
		FAIBFacts Facts;
		Facts.bVitalsKnown = true;
		Facts.bDamageHistoryKnown = true;
		Facts.RecentDamageTakenNorm = 0.5f;
		Facts.bTargetVisible = true;
		Facts.bWeaponCanFight = true;
		Facts.HealthNorm = 1.f;

		// Intact shield: unchanged. Healthy bot, visible enemy, working gun -> fight.
		Facts.ShieldNorm = 1.f;
		TestTag(TEXT("intact shield still fights"), Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Engage);

		// Shield stripped, health untouched. THE case that used to score a dead zero.
		Facts.ShieldNorm = 0.f;
		TestTag(TEXT("broken shield breaks contact"), Engine->Rescore(Facts, 20.0), AIBTags::Ambition_Retreat);

		// And it RELEASES as the shield refills — the want must ease, or this is a one-way
		// exit instead of a rhythm and the bot never comes back to the fight.
		Facts.ShieldNorm = 1.f;
		TestTag(TEXT("recharged shield re-engages"), Engine->Rescore(Facts, 40.0), AIBTags::Ambition_Engage);
	});

	It("leaves SHIELDLESS play exactly as it was — a full shield is neutral", [this]()
	{
		// The safety half, and the one that matters TODAY: the host currently initialises
		// MaxShield to 0 (shields paused, 13 Aug), and its adapter answers ShieldNorm = 1 — a
		// mode without shields must read FULL, never BROKEN, or every bot in it would flee
		// permanently. Pinned on SCORES rather than on the winning tag, because "unchanged"
		// is a claim about the number, not about who happened to win one rescore.
		TArray<FAIBAmbitionSpec> Defaults;
		UAIBAmbitionEngine::BuildDefaultCoreAmbitions(Defaults);
		for (const FAIBAmbitionSpec& Spec : Defaults)
		{
			Engine->RegisterAmbition(Spec);
		}

		FAIBFacts Facts;
		Facts.bVitalsKnown = true;
		Facts.bDamageHistoryKnown = true;
		Facts.RecentDamageTakenNorm = 0.5f;

		// Hurt through health, shield full — the shieldless host's only shape.
		Facts.HealthNorm = 0.3f;
		Facts.ShieldNorm = 1.f;
		Engine->Rescore(Facts, 1.0);
		const float ThroughHealth = RetreatScore();
		TestTrue(TEXT("a hurt shieldless bot still wants out"), ThroughHealth > 0.f);

		// The mirror: same depletion, through the shield instead. min() makes these the
		// SAME danger, which is the whole claim — one layer at H is one layer at H.
		Facts.HealthNorm = 1.f;
		Facts.ShieldNorm = 0.3f;
		Engine->Rescore(Facts, 20.0);
		TestEqual(TEXT("a full shield adds nothing of its own"), RetreatScore(), ThroughHealth, 0.0001f);

		// Whole and shieldless: no want at all. A neutral term must not manufacture one.
		Facts.HealthNorm = 1.f;
		Facts.ShieldNorm = 1.f;
		Engine->Rescore(Facts, 40.0);
		TestEqual(TEXT("whole and shieldless wants no retreat"), RetreatScore(), 0.f, 0.0001f);
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

	// (The barrier's "dry gun must not absorb the match" spec was superseded during the
	//  rebase by the founder's SeekWeapon retirement: the "wants nothing this world
	//  cannot satisfy" spec above pins the same property against the Seek that exists.)

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

	// ---- Phase 6: the translated mode want, end to end and headless -------------------

	It("translates a mode ambition that stays SILENT without its fact and WINS with it", [this]()
	{
		FAIBModeAmbition Mode;
		Mode.AmbitionTag = TAG_AIBSpec_Mode_Child;
		Mode.BaseUtility = 1.2f;

		FAIBAmbitionSpec Translated;
		UAIBAmbitionEngine::BuildModeAmbitionSpec(Mode, Translated);
		Engine->RegisterAmbition(Translated);
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.3f));

		// No objective fact: the translation's ValueWhenUnknown=0 consideration must
		// silence it — a raw registration would score 1.2 forever, which is the
		// CTF-flag-in-Slayer trap the translator exists to close.
		FAIBFacts Facts;
		TestTag(TEXT("factless mode want is silent"),
			Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);

		FAIBObjectiveFact& Objective = Facts.Objectives.AddDefaulted_GetRef();
		Objective.AmbitionTag = TAG_AIBSpec_Mode_Child;
		Objective.Urgency = 0.9f;
		// 1.2 x 0.9 = 1.08 over Roam's 0.3 — and past the incumbent's x1.15 hysteresis.
		TestTag(TEXT("urgent objective wins"), Engine->Rescore(Facts, 10.0), TAG_AIBSpec_Mode_Child);
	});

	It("matches the HOST'S child tag on the Mode gate — and on no exact-match gate", [this]()
	{
		// Headless on purpose: Matches() is plain tag logic, and proving it here is what
		// lets the tree-shape proof in PIE stay a shape proof.
		const FAIBGateModeCondition ModeGate;
		TestTrue(TEXT("Mode gate takes the child"), ModeGate.Matches(TAG_AIBSpec_Mode_Child));
		TestFalse(TEXT("Mode gate refuses Engage"), ModeGate.Matches(AIBTags::Ambition_Engage));

		const FAIBGateEngageCondition EngageGate;
		TestFalse(TEXT("exact gate refuses the child — Mode branches are the ONLY door"),
			EngageGate.Matches(TAG_AIBSpec_Mode_Child));
	});

	It("clears cleanly for re-registration — HasAmbition tells the truth both sides", [this]()
	{
		Engine->RegisterAmbition(Constant(TAG_AIBSpec_Mode_Child, 1.2f));
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.3f));
		TestTrue(TEXT("registered mode want is reported"), Engine->HasAmbition(TAG_AIBSpec_Mode_Child));

		// The RefreshAmbitions contract: a mode leaving must leave NOTHING — a leftover
		// mode want after travel to Slayer is the mirror of the silent-registration trap.
		Engine->ClearAmbitions();
		TestEqual(TEXT("no leftovers after clear"), Engine->NumAmbitions(), 0);
		TestFalse(TEXT("cleared want is gone"), Engine->HasAmbition(TAG_AIBSpec_Mode_Child));
	});

	It("yields when its branch keeps failing — the starvation the Hill measured", [this]()
	{
		// THE DEFECT, as arithmetic. A mode want that wins and then cannot run its branch
		// scores exactly what it scored before, so it wins again, forever. Measured in
		// PIE: seven bots, four minutes, ZERO decisions after the first — 0 kills against
		// 76 in the same build with the objective disabled.
		FAIBModeAmbition Mode;
		Mode.AmbitionTag = TAG_AIBSpec_Mode_Child;
		Mode.BaseUtility = 1.2f;
		FAIBAmbitionSpec Translated;
		UAIBAmbitionEngine::BuildModeAmbitionSpec(Mode, Translated);
		Engine->RegisterAmbition(Translated);
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.3f));

		FAIBFacts Facts;
		FAIBObjectiveFact& Objective = Facts.Objectives.AddDefaulted_GetRef();
		Objective.AmbitionTag = TAG_AIBSpec_Mode_Child;
		Objective.Urgency = 0.9f;

		TestTag(TEXT("the want wins on merit first"),
			Engine->Rescore(Facts, 10.0), TAG_AIBSpec_Mode_Child);

		// The executor reports a branch it could not run.
		Engine->NoteAmbitionFailed(TAG_AIBSpec_Mode_Child, 10.0);
		TestTrue(TEXT("suppressed after the report"),
			Engine->IsAmbitionSuppressed(TAG_AIBSpec_Mode_Child, 10.0));

		// Roam now runs — which is the whole point. Note this also proves THE VETO
		// releases the commit: the mode want was the committed incumbent, and a zero
		// score is the engine's own existing ruling for "declared itself impossible".
		TestTag(TEXT("another want gets a turn"),
			Engine->Rescore(Facts, 11.0), AIBTags::Ambition_Roam);
		for (const FAIBScoredAmbition& Row : Engine->GetLastScores())
		{
			if (Row.Tag == TAG_AIBSpec_Mode_Child)
			{
				// The SCOREBOARD must show the zero too. A suppressed want that still
				// read full utility in the instrument would hide exactly the bug this
				// mechanism exists to kill.
				TestEqual(TEXT("scoreboard shows the suppression"), Row.Score, 0.f, 0.0001f);
			}
		}

		// And it comes BACK — suppression is a silence, never a deletion.
		TestFalse(TEXT("window expires"),
			Engine->IsAmbitionSuppressed(TAG_AIBSpec_Mode_Child, 10.0 + 3.5));
		TestTag(TEXT("recovered want wins again"),
			Engine->Rescore(Facts, 20.0), TAG_AIBSpec_Mode_Child);
	});

	It("escalates repeat failures and forgets old ones", [this]()
	{
		Engine->RegisterAmbition(Constant(TAG_AIBSpec_Mode_Child, 1.2f));

		// Strike 1 -> 3s.  Strike 2 inside the window -> 6s from the SECOND failure.
		Engine->NoteAmbitionFailed(TAG_AIBSpec_Mode_Child, 100.0);
		TestFalse(TEXT("one strike is 3s"),
			Engine->IsAmbitionSuppressed(TAG_AIBSpec_Mode_Child, 103.5));
		Engine->NoteAmbitionFailed(TAG_AIBSpec_Mode_Child, 100.0);
		TestTrue(TEXT("two strikes reach further"),
			Engine->IsAmbitionSuppressed(TAG_AIBSpec_Mode_Child, 105.0));

		// A long clean spell forgets the count, so an occasional failure never
		// accumulates its way into permanent silence.
		Engine->NoteAmbitionFailed(TAG_AIBSpec_Mode_Child, 500.0);
		TestFalse(TEXT("forgotten strikes start over at 3s"),
			Engine->IsAmbitionSuppressed(TAG_AIBSpec_Mode_Child, 503.5));

		// Death clears everything: a respawn must not inherit a dead life's strikes.
		Engine->NoteAmbitionFailed(TAG_AIBSpec_Mode_Child, 600.0);
		Engine->ResetArbitration();
		TestFalse(TEXT("respawn starts clean"),
			Engine->IsAmbitionSuppressed(TAG_AIBSpec_Mode_Child, 600.5));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
