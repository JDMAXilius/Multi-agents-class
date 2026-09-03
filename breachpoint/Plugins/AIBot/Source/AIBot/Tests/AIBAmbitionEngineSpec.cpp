#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Brain/AIBAmbitionEngine.h"
#include "Brain/AIBTactic.h"
#include "Core/AIBBotController.h"
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

	/** Any tag's raw score from the last Rescore — 0 if it did not score at all. */
	float ScoreOf(const UAIBAmbitionEngine& From, FGameplayTag Tag) const
	{
		for (const FAIBScoredAmbition& Row : From.GetLastScores())
		{
			if (Row.Tag == Tag)
			{
				return Row.Score;
			}
		}
		return 0.f;
	}

	/** AIB26: the tactic layer as the controller registers it (FlankCommitSeconds = 3.5). */
	void RegisterTactics(UAIBAmbitionEngine& Into) const
	{
		TArray<FAIBAmbitionSpec> Tactics;
		AIBTactic::BuildDefaultTacticSpecs(Tactics, 3.5f);
		for (const FAIBAmbitionSpec& Spec : Tactics)
		{
			Into.RegisterAmbition(Spec);
		}
	}

	/** The controller's latched flank point, as the tactic engine sees it. */
	static void LatchFlankPoint(FAIBFacts& Facts)
	{
		FAIBObjectiveFact& Point = Facts.Objectives.AddDefaulted_GetRef();
		Point.AmbitionTag = AIBTags::Tactic_Flank;
		Point.Urgency = 1.f;
		Point.DistanceUU = 900.f;
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
		TestEqual(TEXT("six core wants"), Engine->NumAmbitions(), 6); // + Evade (28 Aug)

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

	It("SCATTERS from a grenade at full health, while winning the fight", [this]()
	{
		// THE GAP THIS CLOSES (aib-critic H3, 28 Aug). The whole warning chain — the
		// perceivability trace, the fuse-noise draw, the CanEvadeBlast capability gate, the
		// reaction clock — ended in facts that NOTHING read. BlastCenterRelative had zero
		// readers. A grenade at a bot's feet was answered with the rest of its strafe leg.
		//
		// It is its own want and not a term on Retreat, because considerations MULTIPLY and
		// Retreat's Hurt is exactly 0 above 0.8 vitality: no factor can lift a healthy bot off
		// zero. The bot winning a fight at full health is precisely the one that must move.
		TArray<FAIBAmbitionSpec> Defaults;
		UAIBAmbitionEngine::BuildDefaultCoreAmbitions(Defaults);
		for (const FAIBAmbitionSpec& Spec : Defaults)
		{
			Engine->RegisterAmbition(Spec);
		}

		FAIBFacts Facts;
		Facts.bVitalsKnown = true;
		Facts.HealthNorm = 1.f;          // untouched
		Facts.bWeaponCanFight = true;
		Facts.bHasTarget = true;
		Facts.bTargetVisible = true;     // and winning
		Facts.DistToTargetUU = 600.f;

		// No grenade: the want must be INVISIBLE. Every pre-Evade measurement depends on this.
		TestTag(TEXT("no blast, no change"), Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Engage);

		// A grenade lands at its feet.
		Facts.bIncomingBlast = true;
		Facts.BlastSecondsToDetonation = 0.4f;
		Facts.BlastCenterRelative = FVector(120.f, 0.f, 0.f);
		TestTag(TEXT("a live grenade outranks a fight being won"),
			Engine->Rescore(Facts, 20.0), AIBTags::Ambition_Evade);

		// It goes off / is gone: back to the fight, and the commit must not outlive the blast.
		Facts.bIncomingBlast = false;
		Facts.BlastSecondsToDetonation = 0.f;
		TestTag(TEXT("and the fight resumes after it"),
			Engine->Rescore(Facts, 40.0), AIBTags::Ambition_Engage);
	});

	It("stays silent on a host with no blast seam — the unknown is not a live grenade", [this]()
	{
		// bIncomingBlast false means the selector is UNSET, so Evade scores its
		// ValueWhenUnknown of 0. If that were ever flipped to a number, every bot on a build
		// without grenades would sprint away from nothing, forever, and the cause would look
		// like broken pathing rather than a scoring default.
		TArray<FAIBAmbitionSpec> Defaults;
		UAIBAmbitionEngine::BuildDefaultCoreAmbitions(Defaults);
		for (const FAIBAmbitionSpec& Spec : Defaults)
		{
			Engine->RegisterAmbition(Spec);
		}

		FAIBFacts Facts; // nothing known at all
		Facts.bWeaponCanFight = true;
		const FGameplayTag Won = Engine->Rescore(Facts, 1.0);
		TestTrue(TEXT("a bare facts row never scatters"), Won != AIBTags::Ambition_Evade);
	});

	It("does not punish a want for being well described — the make-up value", [this]()
	{
		// THE UTILITY-AI BIAS this compensates (Dave Mark's IAUS). Multiplying normalised
		// considerations means the more carefully a want is modelled the lower it scores:
		// four terms at 0.8 leave a base-1.0 want at 0.410, while two terms at the same
		// quality keep 0.640. Authors learn to describe less, which is backwards.
		//
		// Pinned as a COMPARISON, not an absolute: two wants of identical base and identical
		// per-term quality, differing only in how many terms they carry.
		const auto Described = [](FGameplayTag Tag, int32 Terms)
		{
			FAIBAmbitionSpec Spec;
			Spec.Tag = Tag;
			Spec.BaseUtility = 1.f;
			for (int32 i = 0; i < Terms; ++i)
			{
				// AmmoNorm is a plain pass-through fact, so each term is exactly the value
				// the facts carry — no curve shaping in the way of the arithmetic.
				FAIBConsideration& C = Spec.Considerations.AddDefaulted_GetRef();
				C.Selector = EAIBFactSelector::AmmoNorm;
				C.SetLinearCurve(true);
			}
			return Spec;
		};

		Engine->RegisterAmbition(Described(AIBTags::Ambition_Engage, 4));
		Engine->RegisterAmbition(Described(AIBTags::Ambition_Roam, 2));

		FAIBFacts Facts;
		Facts.AmmoNorm = 0.8f;
		Engine->Rescore(Facts, 1.0);

		float FourTerms = 0.f, TwoTerms = 0.f;
		for (const FAIBScoredAmbition& Row : Engine->GetLastScores())
		{
			if (Row.Tag == AIBTags::Ambition_Engage) { FourTerms = Row.Score; }
			if (Row.Tag == AIBTags::Ambition_Roam)   { TwoTerms  = Row.Score; }
		}

		// Uncompensated this would be 0.410 vs 0.640 — a 36% penalty for description alone.
		// Compensated the four-term want must stay competitive: within a quarter of the two.
		TestTrue(TEXT("the four-term want is not crushed"), FourTerms > 0.65f * TwoTerms);
		TestTrue(TEXT("but more terms still cost something"), FourTerms < TwoTerms);
	});

	It("keeps a veto a veto, and a certainty a certainty, under compensation", [this]()
	{
		// THE TWO ENDPOINTS ARE WHY THE COMPENSATION IS SAFE HERE, and this is the row that
		// says so. Half this module leans on a consideration of 0 meaning NEVER: a suppressed
		// want, an unknown that must not act, Evade's silence with no blast, Seek's dormancy.
		// If the make-up value ever lifted 0 off the floor, all of those would start firing.
		FAIBAmbitionSpec Vetoed;
		Vetoed.Tag = AIBTags::Ambition_Engage;
		Vetoed.BaseUtility = 10.f; // enormous, so only a true zero can hold it down
		for (int32 i = 0; i < 4; ++i)
		{
			FAIBConsideration& C = Vetoed.Considerations.AddDefaulted_GetRef();
			C.Selector = EAIBFactSelector::AmmoNorm;
			C.SetLinearCurve(true);
		}
		Engine->RegisterAmbition(Vetoed);
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.2f));

		FAIBFacts Facts;
		Facts.AmmoNorm = 0.f; // every term reads zero
		TestTag(TEXT("zero still vetoes, at any base"),
			Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);

		// And the other end: all-perfect terms must not be inflated past the base either.
		Engine->ResetArbitration();
		Facts.AmmoNorm = 1.f;
		Engine->Rescore(Facts, 2.0);
		for (const FAIBScoredAmbition& Row : Engine->GetLastScores())
		{
			if (Row.Tag == AIBTags::Ambition_Engage)
			{
				TestEqual(TEXT("all-perfect terms score exactly the base"), Row.Score, 10.f, 0.001f);
			}
		}
	});

	It("elects NOTHING when every want scores zero — not the first one registered", [this]()
	{
		// Selection used to start at "no best yet", so the FIRST REGISTERED spec won an
		// all-zero scoreboard by default: a bot that "wants Engage at 0.00" and enters a
		// branch which fails on the belief it does not have (aib-critic L3). The tree's
		// ungated Fallback is what should catch this, loudly.
		Engine->RegisterAmbition(VisibleGated(AIBTags::Ambition_Engage, 2.0f));
		Engine->RegisterAmbition(VisibleGated(AIBTags::Ambition_Retreat, 1.0f));

		FAIBFacts Facts; // nothing visible: both gates read zero
		TestFalse(TEXT("no want is elected on an all-zero board"),
			Engine->Rescore(Facts, 1.0).IsValid());
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
		TestEqual(TEXT("still six core wants"), Engine->NumAmbitions(), 6);

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

	It("falls back to Roam on an all-zero board — never the incumbent (AIB22 F5-1b)", [this]()
	{
		// The v4 storm: a Mode want the incumbent, then suppressed; Roam suppressed too;
		// every want at 0 kept the Mode want in the chair and the tree re-entered its
		// branch every frame (`sweep over — 0.0s`, 86k lines a map). The floor takes an
		// all-zero board, its own suppression included — Wander's entry draws fresh.
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.2f));
		Engine->RegisterAmbition(Constant(TAG_AIBSpec_Mode_Child, 1.0f));
		const FAIBFacts Quiet;
		TestTag(TEXT("the mode want wins"), Engine->Rescore(Quiet, 1.0), TAG_AIBSpec_Mode_Child);

		Engine->NoteAmbitionFailed(TAG_AIBSpec_Mode_Child, 2.0);
		Engine->NoteAmbitionFailed(AIBTags::Ambition_Roam, 2.0);
		TestTag(TEXT("every want at zero: the floor, not the incumbent"),
			Engine->Rescore(Quiet, 2.1), AIBTags::Ambition_Roam);
		TestEqual(TEXT("and says fallback"), Engine->GetLastSwitchReason(), EAIBSwitchReason::Fallback);
		TestEqual(TEXT("the name the log prints"),
			FString(UAIBAmbitionEngine::SwitchReasonName(EAIBSwitchReason::Fallback)), FString(TEXT("fallback")));
		TestTrue(TEXT("Roam's own suppression still stands — the board fell through it"),
			Engine->IsAmbitionSuppressed(AIBTags::Ambition_Roam, 2.1));
		TestTag(TEXT("the floor holds while the board stays zero"), Engine->Rescore(Quiet, 2.2), AIBTags::Ambition_Roam);
		TestEqual(TEXT("holding the floor is not a switch"), Engine->GetLastSwitchReason(), EAIBSwitchReason::None);

		// No floor registered: nothing elected — still never the incumbent.
		Engine->ClearAmbitions();
		Engine->ResetArbitration();
		Engine->RegisterAmbition(Constant(TAG_AIBSpec_Mode_Child, 1.0f));
		TestTag(TEXT("the lone want wins"), Engine->Rescore(Quiet, 30.0), TAG_AIBSpec_Mode_Child);
		Engine->NoteAmbitionFailed(TAG_AIBSpec_Mode_Child, 31.0);
		TestFalse(TEXT("no floor: nothing elected, not the incumbent"), Engine->Rescore(Quiet, 31.1).IsValid());
		TestEqual(TEXT("released by veto"), Engine->GetLastSwitchReason(), EAIBSwitchReason::Veto);
	});

	It("refuses a re-issued ABANDONED goal for the window, a different goal at once (AIB22 F5-1a)", [this]()
	{
		// 107k `stall abandoned` lines from ONE clock: the verdict left it running and the
		// branch re-issued the same goal next frame. The controller's locomotion state
		// remembers the abandoned goal, worldless.
		FAIBLocomotionState State;
		const FVector Storey(100.f, 0.f, 300.f);
		TestFalse(TEXT("nothing abandoned yet"), State.RefusesGoal(Storey, 1.0, 50.f));
		State.NoteAbandoned(Storey, 1.0, /*WindowSeconds=*/3.f);
		TestTrue(TEXT("the same goal next frame is refused"), State.RefusesGoal(Storey, 1.02, 50.f));
		TestTrue(TEXT("inside the goal tolerance is the same goal"), State.RefusesGoal(Storey + FVector(20.f, 0.f, 0.f), 2.0, 50.f));
		TestFalse(TEXT("a different goal runs its own clock"), State.RefusesGoal(FVector(900.f, 0.f, 300.f), 2.0, 50.f));
		TestFalse(TEXT("the window lapses"), State.RefusesGoal(Storey, 4.0, 50.f));
	});

	// ---- AIB26 / Phase 15: the switch-reason instrument, the replay fingerprint, the
	//      tactic layer on the second engine, and determinism ----------------------------

	It("names WHY the winner changed — first, merit, veto, interrupt", [this]()
	{
		// The utility-pathology instrument: a dither is a run of `merit`, a Flank that
		// kills its own commit is `veto` with a commit live, a state-shaped interrupt
		// would read `interrupt` every think. Each cause, produced on purpose.
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f));
		const FAIBFacts Quiet;
		Engine->Rescore(Quiet, 1.0);
		TestEqual(TEXT("a fresh life's first pick"), Engine->GetLastSwitchReason(), EAIBSwitchReason::First);
		Engine->Rescore(Quiet, 1.1);
		TestEqual(TEXT("holding is not a switch"), Engine->GetLastSwitchReason(), EAIBSwitchReason::None);

		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Search, 1.3f));
		Engine->Rescore(Quiet, 2.0);
		TestEqual(TEXT("beaten through the hysteresis"), Engine->GetLastSwitchReason(), EAIBSwitchReason::Merit);

		// A committed incumbent that scores zero releases itself: veto, not merit.
		Engine->ClearAmbitions();
		Engine->RegisterAmbition(VisibleGated(AIBTags::Ambition_Engage, 2.0f, /*Commit=*/5.f));
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 0.2f));
		FAIBFacts Facts;
		Facts.bTargetVisible = true;
		Engine->Rescore(Facts, 10.0);
		Facts.bTargetVisible = false;
		TestTag(TEXT("the vetoed commit releases"), Engine->Rescore(Facts, 11.0), AIBTags::Ambition_Roam);
		TestEqual(TEXT("and says veto"), Engine->GetLastSwitchReason(), EAIBSwitchReason::Veto);

		// A live commit voided by the blast's edge: interrupt.
		Engine->ClearAmbitions();
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Roam, 1.0f, /*Commit=*/5.f));
		FAIBFacts Blast;
		Engine->Rescore(Blast, 20.0);
		Engine->RegisterAmbition(Constant(AIBTags::Ambition_Retreat, 5.0f));
		Blast.bIncomingBlast = true;
		Blast.BlastSecondsToDetonation = 1.0f;
		TestTag(TEXT("the edge switches"), Engine->Rescore(Blast, 21.0), AIBTags::Ambition_Retreat);
		TestEqual(TEXT("and says interrupt"), Engine->GetLastSwitchReason(), EAIBSwitchReason::Interrupt);
		TestEqual(TEXT("the name the log prints"), FString(UAIBAmbitionEngine::SwitchReasonName(EAIBSwitchReason::Interrupt)), FString(TEXT("interrupt")));
	});

	It("fingerprints the facts at 3 dp — noise agrees, a real change does not", [this]()
	{
		// The `facts=` field of the decide line. Two seeded runs whose floats differ in
		// the last bits must still diff empty; a change the curves can see must not.
		FAIBFacts A;
		A.bVitalsKnown = true;
		A.HealthNorm = 0.75f;
		A.DistToTargetUU = 812.f;
		FAIBObjectiveFact& Objective = A.Objectives.AddDefaulted_GetRef();
		Objective.AmbitionTag = AIBTags::Ambition_Mode;
		Objective.Urgency = 0.4f;

		FAIBFacts B = A;
		TestEqual(TEXT("identical facts, identical crc"), UAIBAmbitionEngine::FactsCrc32(A), UAIBAmbitionEngine::FactsCrc32(B));
		B.HealthNorm += 0.0002f;
		TestEqual(TEXT("sub-3dp noise is invisible"), UAIBAmbitionEngine::FactsCrc32(A), UAIBAmbitionEngine::FactsCrc32(B));
		B.HealthNorm = 0.76f;
		TestNotEqual(TEXT("a 3dp change shows"), UAIBAmbitionEngine::FactsCrc32(A), UAIBAmbitionEngine::FactsCrc32(B));

		FAIBFacts C = A;
		C.Objectives[0].AmbitionTag = AIBTags::Ambition_Seek;
		TestNotEqual(TEXT("the objective's tag is part of the fingerprint"), UAIBAmbitionEngine::FactsCrc32(A), UAIBAmbitionEngine::FactsCrc32(C));
		FAIBFacts D = A;
		D.bTargetVisible = true;
		TestNotEqual(TEXT("a flag flip shows"), UAIBAmbitionEngine::FactsCrc32(A), UAIBAmbitionEngine::FactsCrc32(D));
	});

	It("always elects a tactic — Push is a floor no fact can zero", [this]()
	{
		// The tree's Push child is UNGATED and last so Engage always has a child; that is
		// only honest if the engine never leaves the tactic board empty. The worst facts:
		// no nerve, no vitality, a target at full health, nothing latched.
		RegisterTactics(*Engine);
		TestEqual(TEXT("three tactics"), Engine->NumAmbitions(), 3);
		FAIBFacts Worst;
		Worst.bVitalsKnown = true;
		Worst.HealthNorm = 0.f;
		Worst.ShieldNorm = 0.f;
		Worst.bConfidenceKnown = true;
		Worst.ConfidenceNorm = 0.f;
		Worst.bTargetVitalsKnown = true;
		Worst.TargetHealthNorm = 1.f;
		Worst.AmmoNorm = 1.f;
		TestTag(TEXT("Push holds the floor"), Engine->Rescore(Worst, 1.0), AIBTags::Tactic_Push);
		TestTrue(TEXT("and it is not zero"), ScoreOf(*Engine, AIBTags::Tactic_Push) > 0.f);
	});

	It("scores Flank to ZERO only through the latched point — the VETO-bypass rule", [this]()
	{
		// W-AUDIT P15: "Flank's gate must return 0 only on a LATCHED failure or it dithers
		// through its own commit". Every other term floors above 0, by construction —
		// pinned with the most hostile per-think facts the curves can be handed.
		RegisterTactics(*Engine);

		FAIBFacts Ideal;
		Ideal.bHasTarget = true;
		Ideal.bTargetVisible = true;
		Ideal.DistToTargetUU = 800.f;          // the flank's home band
		Ideal.bDamageHistoryKnown = true;
		Ideal.RecentDamageTakenNorm = 0.5f;    // being shot: go around
		Ideal.bCrowdKnown = true;
		Ideal.NearbyAllies = 2;                // teammates on the target
		TestTag(TEXT("no latched point: silent, Push fights"), Engine->Rescore(Ideal, 1.0), AIBTags::Tactic_Push);
		TestEqual(TEXT("Flank reads exactly zero"), ScoreOf(*Engine, AIBTags::Tactic_Flank), 0.f, 0.0001f);

		LatchFlankPoint(Ideal);
		TestTag(TEXT("the point latched: Flank wins"), Engine->Rescore(Ideal, 2.0), AIBTags::Tactic_Flank);

		FAIBFacts Hostile;
		Hostile.bHasTarget = true;
		Hostile.DistToTargetUU = 50.f;         // knife range
		Hostile.bDamageHistoryKnown = true;
		Hostile.RecentDamageTakenNorm = 0.f;   // untouched
		Hostile.bCrowdKnown = true;
		Hostile.NearbyAllies = 0;              // alone
		LatchFlankPoint(Hostile);
		Engine->ResetArbitration();
		Engine->Rescore(Hostile, 3.0);
		TestTrue(TEXT("hostile per-think facts cannot zero a latched Flank"), ScoreOf(*Engine, AIBTags::Tactic_Flank) > 0.f);
	});

	It("holds Flank through its commit while the point stands, and releases the moment it is gone", [this]()
	{
		RegisterTactics(*Engine);
		FAIBFacts Facts;
		Facts.bHasTarget = true;
		Facts.DistToTargetUU = 800.f;
		Facts.bDamageHistoryKnown = true;
		Facts.RecentDamageTakenNorm = 0.5f;
		LatchFlankPoint(Facts);
		TestTag(TEXT("Flank wins"), Engine->Rescore(Facts, 10.0), AIBTags::Tactic_Flank); // committed to 13.5

		// Mid-manoeuvre the facts turn against it — closer, no fresh damage — and Push
		// would win on merit. The commit holds: the point is still latched.
		Facts.DistToTargetUU = 200.f;
		Facts.RecentDamageTakenNorm = 0.f;
		TestTag(TEXT("the commit holds through a bad tick"), Engine->Rescore(Facts, 11.0), AIBTags::Tactic_Flank);
		TestTrue(TEXT("the commit is live"), Engine->GetCommitEndSeconds() > 11.0);

		// The controller clears the latch (arrived / refused / stalled): the ONE zero.
		Facts.Objectives.Reset();
		TestTag(TEXT("no point: Push, at once"), Engine->Rescore(Facts, 12.0), AIBTags::Tactic_Push);
		TestEqual(TEXT("released by veto, inside the window"), Engine->GetLastSwitchReason(), EAIBSwitchReason::Veto);
	});

	It("wants Hold with a thin magazine at range on the high ground — and Push up close and full", [this]()
	{
		RegisterTactics(*Engine);
		FAIBFacts Facts;
		Facts.bHasTarget = true;
		Facts.AmmoNorm = 1.f;
		Facts.DistToTargetUU = 300.f;
		Facts.HeightAdvantageUU = 0.f;
		TestTag(TEXT("full and close: Push"), Engine->Rescore(Facts, 1.0), AIBTags::Tactic_Push);

		Engine->ResetArbitration();
		Facts.AmmoNorm = 0.1f;
		Facts.DistToTargetUU = 900.f;
		Facts.HeightAdvantageUU = 300.f;
		TestTag(TEXT("thin, at range, above: Hold"), Engine->Rescore(Facts, 2.0), AIBTags::Tactic_Hold);
	});

	It("ends a Hold by the controller's clock, never by a term — bounded stillness (F9)", [this]()
	{
		// The controller's Hold task reports HoldMaxSeconds reached through the same
		// failure-suppression door every branch uses; the engine's job is to move on and
		// let Hold back only after its rest.
		RegisterTactics(*Engine);
		FAIBFacts Facts;
		Facts.bHasTarget = true;
		Facts.AmmoNorm = 0.1f;
		Facts.DistToTargetUU = 900.f;
		Facts.HeightAdvantageUU = 300.f;
		TestTag(TEXT("Hold wins"), Engine->Rescore(Facts, 10.0), AIBTags::Tactic_Hold);
		Engine->NoteAmbitionFailed(AIBTags::Tactic_Hold, 14.0); // HoldMaxSeconds reached
		TestTag(TEXT("the hold is over: Push"), Engine->Rescore(Facts, 14.1), AIBTags::Tactic_Push);
		TestTag(TEXT("Hold comes back after its rest"), Engine->Rescore(Facts, 18.0), AIBTags::Tactic_Hold);
	});

	It("replays byte-identical decisions for the same facts stream — the seeded-diff premise", [this]()
	{
		// The `decide` line diffs empty across two `-AIBSeed=N` runs only if the engine
		// itself is a pure function of (registry, facts stream, events, clock). Two engines,
		// one scripted stream with a suppression event in it, compared field by field
		// exactly as the line prints them.
		UAIBAmbitionEngine* Twin = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient);
		Twin->AddToRoot();
		for (UAIBAmbitionEngine* E : { Engine, Twin })
		{
			TArray<FAIBAmbitionSpec> Defaults;
			UAIBAmbitionEngine::BuildDefaultCoreAmbitions(Defaults);
			for (const FAIBAmbitionSpec& Spec : Defaults)
			{
				E->RegisterAmbition(Spec);
			}
		}

		const auto Step = [](UAIBAmbitionEngine& E, const FAIBFacts& F, double T)
		{
			const FGameplayTag Won = E.Rescore(F, T);
			return FString::Printf(TEXT("%s s=%.3f over=%s rs=%.3f reason=%s facts=%08x"),
				*Won.ToString(), E.GetCurrentScore(), *E.GetLastRunnerUp().Tag.ToString(),
				E.GetLastRunnerUp().Score, UAIBAmbitionEngine::SwitchReasonName(E.GetLastSwitchReason()),
				UAIBAmbitionEngine::FactsCrc32(F));
		};

		FAIBFacts F;
		F.bVitalsKnown = true;
		F.HealthNorm = 1.f;
		F.bWeaponCanFight = true;
		bool bIdentical = true;
		for (int32 Tick = 0; Tick < 60; ++Tick)
		{
			const double T = 1.0 + Tick * 0.1;
			// A scripted fight: contact at 1 s, wounded at 3 s, lost at 4 s, a branch
			// failure at 4.5 s, blast at 5 s.
			F.bHasTarget = Tick >= 10 && Tick < 40;
			F.bTargetVisible = F.bHasTarget;
			F.DistToTargetUU = F.bHasTarget ? 900.f - Tick * 10.f : -1.f;
			F.HealthNorm = Tick >= 30 ? 0.3f : 1.f;
			F.bHasMemory = Tick >= 40;
			F.LastKnownAgeSeconds = Tick >= 40 ? (Tick - 40) * 0.1f : 0.f;
			F.MemoryFreshWindowSeconds = 16.f;
			F.bIncomingBlast = Tick >= 50;
			F.BlastSecondsToDetonation = F.bIncomingBlast ? 2.f - (Tick - 50) * 0.1f : 0.f;
			if (Tick == 45)
			{
				Engine->NoteAmbitionFailed(AIBTags::Ambition_Search, T);
				Twin->NoteAmbitionFailed(AIBTags::Ambition_Search, T);
			}
			bIdentical &= (Step(*Engine, F, T) == Step(*Twin, F, T));
		}
		TestTrue(TEXT("sixty decisions, no divergence"), bIdentical);
		Twin->RemoveFromRoot();
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
