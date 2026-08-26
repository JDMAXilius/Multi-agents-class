#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Brain/AIBAmbitionEngine.h"
#include "Brain/AIBConfidenceModel.h"
#include "Core/AIBTags.h"
#include "Core/AIBTypes.h"
#include "Math/RandomStream.h"

/**
 * Phase 5, proven headless: the momentum ledger's decay, the assessment's directions,
 * the HELD misjudge (competence = judgment quality), and the one behavioural claim the
 * roadmap names — confidence swings Engage <-> Retreat ("observed disengage/press",
 * pinned here as arithmetic before the terminal observes it live).
 */
BEGIN_DEFINE_SPEC(FAIBConfidenceSpec, "AIBot.Sim.Confidence",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** A fight read the assessor scores mid-way: healthy-ish, armed, seen enemy. */
	FAIBFacts BaselineFacts() const
	{
		FAIBFacts Facts;
		Facts.bVitalsKnown = true;
		Facts.HealthNorm = 0.5f;
		Facts.bWeaponCanFight = true;
		return Facts;
	}

	bool TestTag(const TCHAR* What, FGameplayTag Actual, FGameplayTag Expected)
	{
		return TestEqual(What, Actual.GetTagName(), Expected.GetTagName());
	}

END_DEFINE_SPEC(FAIBConfidenceSpec)

void FAIBConfidenceSpec::Define()
{
	Describe("the damage ledger (momentum)", [this]()
	{
		It("decays on the half-life exactly — one number, one law", [this]()
		{
			FAIBDamageLedger Ledger;
			Ledger.NoteTaken(1.0f, 10.0);
			TestEqual(TEXT("fresh"), Ledger.TakenNorm(10.0), 1.0f, 0.001f);
			TestEqual(TEXT("one half-life"), Ledger.TakenNorm(10.0 + FAIBDamageLedger::HalfLifeSeconds), 0.5f, 0.005f);
			TestEqual(TEXT("two half-lives"), Ledger.TakenNorm(10.0 + 2.0 * FAIBDamageLedger::HalfLifeSeconds), 0.25f, 0.005f);
		});

		It("accumulates a burst on top of the decayed past, per side", [this]()
		{
			FAIBDamageLedger Ledger;
			Ledger.NoteTaken(0.4f, 0.0);
			Ledger.NoteTaken(0.4f, FAIBDamageLedger::HalfLifeSeconds); // 0.2 remains + 0.4
			TestEqual(TEXT("taken sums with decay"), Ledger.TakenNorm(FAIBDamageLedger::HalfLifeSeconds), 0.6f, 0.005f);
			TestEqual(TEXT("dealt is a separate book"), Ledger.DealtNorm(FAIBDamageLedger::HalfLifeSeconds), 0.f, 0.001f);

			Ledger.NoteDealt(0.3f, 20.0);
			TestEqual(TEXT("dealt records"), Ledger.DealtNorm(20.0), 0.3f, 0.001f);
		});

		It("refuses heals and garbage — negative and NaN never move a book", [this]()
		{
			FAIBDamageLedger Ledger;
			Ledger.NoteTaken(-0.5f, 1.0);
			Ledger.NoteTaken(FMath::Sqrt(-1.f), 1.0); // NaN
			TestEqual(TEXT("still empty"), Ledger.TakenNorm(1.0), 0.f, 0.001f);
		});

		It("resets to nothing — momentum dies with the body", [this]()
		{
			FAIBDamageLedger Ledger;
			Ledger.NoteTaken(1.f, 5.0);
			Ledger.NoteDealt(1.f, 5.0);
			Ledger.Reset();
			TestEqual(TEXT("taken gone"), Ledger.TakenNorm(5.0), 0.f, 0.001f);
			TestEqual(TEXT("dealt gone"), Ledger.DealtNorm(5.0), 0.f, 0.001f);
		});
	});

	Describe("the assessment (level-invariant true read)", [this]()
	{
		It("reads a winning fight high and a losing one low — every input pulls its way", [this]()
		{
			FAIBFacts Winning = BaselineFacts();
			Winning.HealthNorm = 1.f;
			Winning.bDamageHistoryKnown = true;
			Winning.RecentDamageDealtNorm = 0.8f;
			Winning.RecentDamageTakenNorm = 0.1f;

			FAIBFacts Losing = BaselineFacts();
			Losing.HealthNorm = 0.15f;
			Losing.bWeaponCanFight = false;
			Losing.bDamageHistoryKnown = true;
			Losing.RecentDamageDealtNorm = 0.f;
			Losing.RecentDamageTakenNorm = 0.9f;
			Losing.bCrowdKnown = true; // the outnumbered term needs an honest crowd read
			Losing.NearbyEnemies = 2;

			const float High = FAIBConfidenceModel::Assess(Winning);
			const float Low = FAIBConfidenceModel::Assess(Losing);
			TestTrue(TEXT("winning reads high"), High > 0.75f);
			TestTrue(TEXT("losing reads low"), Low < 0.2f);
			TestTrue(TEXT("both clamped"), High <= 1.f && Low >= 0.f);
		});

		It("treats unknowns as absent — a broken adapter is not a wounded bot", [this]()
		{
			// All-unknown row: vitals and history contribute nothing; the one KNOWN fact
			// is the helpless hand (bWeaponCanFight defaults false), which reads DOWN.
			const float Unknowns = FAIBConfidenceModel::Assess(FAIBFacts());
			TestEqual(TEXT("0.5 base minus only the helpless hand"), Unknowns, 0.3f, 0.001f);

			// The same row with vitals KNOWN at half health scores identically — known
			// average adds nothing over unknown, which is the honest indifference point.
			FAIBFacts HalfKnown;
			HalfKnown.bVitalsKnown = true;
			HalfKnown.HealthNorm = 0.5f;
			TestEqual(TEXT("known-average equals unknown"), FAIBConfidenceModel::Assess(HalfKnown), 0.3f, 0.001f);
		});

		It("weighs momentum symmetrically around an even trade", [this]()
		{
			FAIBFacts Even = BaselineFacts();
			Even.bDamageHistoryKnown = true;
			Even.RecentDamageDealtNorm = 0.5f;
			Even.RecentDamageTakenNorm = 0.5f;
			const float EvenRead = FAIBConfidenceModel::Assess(Even);

			FAIBFacts NoHistory = BaselineFacts();
			TestEqual(TEXT("an even trade reads exactly like no history"),
				EvenRead, FAIBConfidenceModel::Assess(NoHistory), 0.001f);
		});
	});

	Describe("the judgment ladder (competence = how WELL, never what)", [this]()
	{
		It("holds a misjudge between draws — a bad call, not a broken needle", [this]()
		{
			FAIBConfidenceState State;
			FRandomStream Rng; Rng.Initialize(42);
			const FAIBFacts Facts = BaselineFacts();
			const float First = FAIBConfidenceModel::Step(State, Facts, EAIBCompetence::Novice, Rng, 0.0);
			// Inside the hold window: same facts, same answer, no draw consumed.
			const int32 SeedBefore = Rng.GetCurrentSeed();
			const float Second = FAIBConfidenceModel::Step(State, Facts, EAIBCompetence::Novice, Rng, 0.5);
			TestEqual(TEXT("held"), Second, First, 0.0001f);
			TestEqual(TEXT("no draw consumed inside the hold"), Rng.GetCurrentSeed(), SeedBefore);
		});

		It("moves WITH the facts inside a hold — the offset is held, not the answer", [this]()
		{
			FAIBConfidenceState State;
			FRandomStream Rng; Rng.Initialize(42);
			FAIBFacts Facts = BaselineFacts();
			const float Healthy = FAIBConfidenceModel::Step(State, Facts, EAIBCompetence::Expert, Rng, 0.0);
			Facts.HealthNorm = 0.1f; // shot to pieces inside the same judge window
			const float Wounded = FAIBConfidenceModel::Step(State, Facts, EAIBCompetence::Expert, Rng, 0.1);
			TestTrue(TEXT("the read drops with the facts"), Wounded < Healthy - 0.1f);
		});

		It("re-judges on the cadence, and a Novice's wrongness is wider than an Expert's", [this]()
		{
			const FAIBFacts Facts = BaselineFacts();
			const float TrueRead = FAIBConfidenceModel::Assess(Facts);

			float NoviceWorst = 0.f, ExpertWorst = 0.f;
			bool bSawRedraw = false;
			float NoviceFirst = -1.f;
			for (int32 Seed = 0; Seed < 32; ++Seed)
			{
				FAIBConfidenceState NoviceState, ExpertState;
				FRandomStream NoviceRng, ExpertRng; NoviceRng.Initialize(Seed); ExpertRng.Initialize(Seed);
				// Step far past the cadence each time so every step is a fresh draw.
				for (int32 Draw = 0; Draw < 8; ++Draw)
				{
					const double Now = Draw * 10.0;
					const float N = FAIBConfidenceModel::Step(NoviceState, Facts, EAIBCompetence::Novice, NoviceRng, Now);
					const float E = FAIBConfidenceModel::Step(ExpertState, Facts, EAIBCompetence::Expert, ExpertRng, Now);
					NoviceWorst = FMath::Max(NoviceWorst, FMath::Abs(N - TrueRead));
					ExpertWorst = FMath::Max(ExpertWorst, FMath::Abs(E - TrueRead));
					if (NoviceFirst < 0.f) { NoviceFirst = N; }
					else if (FMath::Abs(N - NoviceFirst) > 0.001f) { bSawRedraw = true; }
				}
			}
			TestTrue(TEXT("the cadence re-draws (the read moves across judges)"), bSawRedraw);
			TestTrue(TEXT("a Novice can be badly wrong"), NoviceWorst > 0.15f);
			TestTrue(TEXT("an Expert never strays far"),
				ExpertWorst <= FAIBConfidenceModel::MisjudgeAmplitude(EAIBCompetence::Expert) + 0.001f);
			TestTrue(TEXT("the ladder orders the wrongness"), ExpertWorst < NoviceWorst);
		});

		It("keeps the ladder monotone in both knobs", [this]()
		{
			const EAIBCompetence Rungs[] = { EAIBCompetence::Novice, EAIBCompetence::Trained,
				EAIBCompetence::Skilled, EAIBCompetence::Expert };
			for (int32 i = 1; i < 4; ++i)
			{
				TestTrue(TEXT("misjudge narrows every rung"),
					FAIBConfidenceModel::MisjudgeAmplitude(Rungs[i]) < FAIBConfidenceModel::MisjudgeAmplitude(Rungs[i - 1]));
				TestTrue(TEXT("re-judge quickens every rung"),
					FAIBConfidenceModel::ReJudgeSeconds(Rungs[i]) < FAIBConfidenceModel::ReJudgeSeconds(Rungs[i - 1]));
			}
		});

		It("repeats exactly under one seed — a replayable judgment", [this]()
		{
			const FAIBFacts Facts = BaselineFacts();
			FAIBConfidenceState A, B;
			FRandomStream RngA, RngB; RngA.Initialize(7); RngB.Initialize(7);
			for (int32 Draw = 0; Draw < 5; ++Draw)
			{
				const double Now = Draw * 10.0;
				TestEqual(TEXT("identical stream, identical read"),
					FAIBConfidenceModel::Step(A, Facts, EAIBCompetence::Trained, RngA, Now),
					FAIBConfidenceModel::Step(B, Facts, EAIBCompetence::Trained, RngB, Now), 0.0001f);
			}
		});
	});

	Describe("confidence wired into the ambitions (disengage <-> press, as arithmetic)", [this]()
	{
		It("flips a wounded mid-fight bot between Engage and Retreat on confidence alone", [this]()
		{
			UAIBAmbitionEngine* Engine = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient);
			TArray<FAIBAmbitionSpec> Defaults;
			UAIBAmbitionEngine::BuildDefaultCoreAmbitions(Defaults);
			for (const FAIBAmbitionSpec& Spec : Defaults)
			{
				Engine->RegisterAmbition(Spec);
			}

			// The knife's-edge row — RE-PINNED (W-REVIEW P4+5 M1): the old row
			// (H=0.4, d=1400) asserted a disengage at ConfidenceNorm 0.05, a value the
			// model's own misjudge floor (0.125 at those facts) can NEVER produce —
			// green spec, unreachable feature. This row is the reviewer's worked
			// reachable example: Assess = 0.375, a Trained misjudge spans C in
			// [0.195, 0.555], and BOTH injected values below sit inside that span —
			// the axis is injected (this spec pins the ENGINE's response, the model
			// spec pins the producer), but every number is one the producer can make.
			FAIBFacts Facts;
			Facts.bVitalsKnown = true;
			Facts.HealthNorm = 0.30f;
			Facts.bWeaponCanFight = true;
			Facts.bHasTarget = true;
			Facts.bTargetVisible = true;
			Facts.DistToTargetUU = 1200.f;
			Facts.bDamageHistoryKnown = true;
			Facts.RecentDamageTakenNorm = 0.5f;
			Facts.bConfidenceKnown = true;

			Facts.ConfidenceNorm = 0.55f; // top of the reachable span — above the band
			TestTag(TEXT("a confident bot PRESSES the same fight"),
				Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Engage);

			Engine->ResetArbitration();
			Facts.ConfidenceNorm = 0.20f; // bottom of the span — below the band
			TestTag(TEXT("a shaken bot DISENGAGES from it"),
				Engine->Rescore(Facts, 2.0), AIBTags::Ambition_Retreat);
		});

		It("changes nothing when confidence is unknown — every pre-Phase-5 pin holds", [this]()
		{
			// Both nerve considerations answer 1.0 on unknown by design; this pins that
			// the wiring is invisible to a host without the seam.
			UAIBAmbitionEngine* Engine = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient);
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
			Facts.DistToTargetUU = 800.f;
			TestTag(TEXT("healthy armed and seeing still engages"),
				Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Engage);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
