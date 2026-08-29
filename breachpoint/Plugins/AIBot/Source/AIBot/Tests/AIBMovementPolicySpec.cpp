#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTypes.h"
#include "Math/RandomStream.h"
#include "Skills/AIBMovementPolicy.h"

/**
 * The strafe ladder, proven worldless: a state struct, a seeded stream, and time as a
 * number — the ambition suite's shape, because the policy has exactly the same shape.
 *
 * What these pins are FOR. A competence ladder is only real if it is observable, so the
 * behavioural specs run seed SWEEPS and assert on rates, not on single lucky draws: one
 * seed proves nothing about a 0.05 chance, and a spec that could pass with the ladder
 * deleted is worse than no spec (the sensorium suite's F-5.1 lesson, applied early).
 * Every rate assertion also pins its own SAMPLE SIZE, so none of them can pass vacuously
 * on an empty loop.
 *
 * The pinned ladder (any retune that changes these must change this file, on purpose):
 *   Chance    0.05 / 0.40 / 0.75 / 0.95
 *   Leg secs  1.20-2.00 / 0.80-1.60 / 0.55-1.20 / 0.35-0.90
 *   Juke      0.00 / 0.00 / 0.25 / 0.50   <- capability-shaped, exactly zero below Skilled
 */
BEGIN_DEFINE_SPEC(FAIBMovementPolicySpec, "AIBot.Sim.MovementPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** TestEqual has no EAIBStrafeIntent overload. Comparing names takes the FString one,
	 *  which PRINTS both sides on failure — "Left != Right" costs the next session far
	 *  less than "0 != 1". Same reasoning as the ambition suite's TestTag. */
	FString IntentName(EAIBStrafeIntent Intent) const
	{
		switch (Intent)
		{
		case EAIBStrafeIntent::Left:  return TEXT("Left");
		case EAIBStrafeIntent::Right: return TEXT("Right");
		default:                      return TEXT("Hold");
		}
	}

	bool TestIntent(const TCHAR* What, EAIBStrafeIntent Actual, EAIBStrafeIntent Expected)
	{
		return TestEqual(What, IntentName(Actual), IntentName(Expected));
	}

	/**
	 * Drives N DECISION WINDOWS at one competence and one seed, collecting the intent each
	 * window settled on. Each step jumps ten seconds — longer than the longest leg at any
	 * rung (2.00s) — so every call lands past the deadline and every call is a decision.
	 */
	void RunWindows(EAIBCompetence Level, int32 Seed, int32 Windows,
		TArray<EAIBStrafeIntent>& OutIntents) const
	{
		FRandomStream Rng;
		Rng.Initialize(Seed);

		FAIBMovementState State;
		double Now = 0.0;

		OutIntents.Reset();
		OutIntents.Reserve(Windows);
		for (int32 Index = 0; Index < Windows; ++Index)
		{
			OutIntents.Add(FAIBMovementPolicy::StepStrafe(State, Level, Rng, Now));
			Now += 10.0;
		}
	}

	/** The share of decision windows that strafed at all, over a seed sweep. */
	float StrafeRate(EAIBCompetence Level, int32 Seeds, int32 Windows, int32& OutSamples) const
	{
		int32 Strafed = 0;
		OutSamples = 0;

		TArray<EAIBStrafeIntent> Intents;
		for (int32 Seed = 1; Seed <= Seeds; ++Seed)
		{
			RunWindows(Level, Seed, Windows, Intents);
			for (const EAIBStrafeIntent Intent : Intents)
			{
				++OutSamples;
				Strafed += (Intent != EAIBStrafeIntent::Hold) ? 1 : 0;
			}
		}
		return OutSamples > 0 ? static_cast<float>(Strafed) / static_cast<float>(OutSamples) : 0.f;
	}

	/**
	 * The REVERSAL harness. An opportunity is a pair of adjacent decision windows that
	 * BOTH strafed — only then is there a previous direction to reverse off. Returns the
	 * share of those opportunities that changed side.
	 *
	 * Honest reading of this number: the policy's no-juke branch is an even pick, so a
	 * reversal happens by coin half the time at every rung. The juke ADDS to that. So the
	 * measurable claim is a BIAS — 0.50 below Skilled, 0.625 at Skilled, 0.75 at Expert —
	 * and the exact "zero at any tuning value" capability is pinned separately, straight
	 * off the constant. Both together are the capability gate; neither alone is.
	 */
	float ReversalRate(EAIBCompetence Level, int32 Seeds, int32 Windows, int32& OutOpportunities) const
	{
		int32 Reversals = 0;
		OutOpportunities = 0;

		TArray<EAIBStrafeIntent> Intents;
		for (int32 Seed = 1; Seed <= Seeds; ++Seed)
		{
			RunWindows(Level, Seed, Windows, Intents);
			for (int32 Index = 1; Index < Intents.Num(); ++Index)
			{
				const EAIBStrafeIntent Before = Intents[Index - 1];
				const EAIBStrafeIntent After = Intents[Index];
				if (Before == EAIBStrafeIntent::Hold || After == EAIBStrafeIntent::Hold)
				{
					continue;
				}
				++OutOpportunities;
				Reversals += (Before != After) ? 1 : 0;
			}
		}
		return OutOpportunities > 0 ? static_cast<float>(Reversals) / static_cast<float>(OutOpportunities) : 0.f;
	}

END_DEFINE_SPEC(FAIBMovementPolicySpec)

void FAIBMovementPolicySpec::Define()
{
	It("keeps the strafe ladder monotone — Novice <= Trained <= Skilled <= Expert", [this]()
	{
		const float Novice = FAIBMovementPolicy::StrafeChance(EAIBCompetence::Novice);
		const float Trained = FAIBMovementPolicy::StrafeChance(EAIBCompetence::Trained);
		const float Skilled = FAIBMovementPolicy::StrafeChance(EAIBCompetence::Skilled);
		const float Expert = FAIBMovementPolicy::StrafeChance(EAIBCompetence::Expert);

		TestTrue(TEXT("Novice <= Trained"), Novice <= Trained);
		TestTrue(TEXT("Trained <= Skilled"), Trained <= Skilled);
		TestTrue(TEXT("Skilled <= Expert"), Skilled <= Expert);

		// Monotone is not enough: rungs inside each other's noise buy no read at all.
		TestTrue(TEXT("the rungs are SEPARATED, not merely ordered"), Expert - Novice > 0.5f);

		// Every chance is a probability, or the roll comparison is meaningless.
		TestTrue(TEXT("Novice in 0..1"), Novice >= 0.f && Novice <= 1.f);
		TestTrue(TEXT("Expert in 0..1"), Expert >= 0.f && Expert <= 1.f);

		// The pinned values themselves — a retune must come here and say so.
		TestEqual(TEXT("Novice chance"), Novice, 0.05f, 0.0001f);
		TestEqual(TEXT("Trained chance"), Trained, 0.40f, 0.0001f);
		TestEqual(TEXT("Skilled chance"), Skilled, 0.75f, 0.0001f);
		TestEqual(TEXT("Expert chance"), Expert, 0.95f, 0.0001f);
	});

	It("orders every leg's bounds Min <= Max, and tightens the cadence as competence rises", [this]()
	{
		const EAIBCompetence Ladder[4] = { EAIBCompetence::Novice, EAIBCompetence::Trained,
			EAIBCompetence::Skilled, EAIBCompetence::Expert };

		for (int32 Index = 0; Index < 4; ++Index)
		{
			const float Min = FAIBMovementPolicy::StrafeLegSecondsMin(Ladder[Index]);
			const float Max = FAIBMovementPolicy::StrafeLegSecondsMax(Ladder[Index]);
			TestTrue(TEXT("Min is positive — a zero leg would re-decide every tick"), Min > 0.f);
			TestTrue(TEXT("Min <= Max — FRandRange's arguments are in order"), Min <= Max);
			TestTrue(TEXT("the band is WIDE — a fixed leg is a metronome, which is a tell"),
				Max - Min > 0.2f);

			if (Index > 0)
			{
				// Faster hands, shorter legs: a long slow leg is trivially led.
				TestTrue(TEXT("Min tightens with the rung"),
					Min <= FAIBMovementPolicy::StrafeLegSecondsMin(Ladder[Index - 1]));
				TestTrue(TEXT("Max tightens with the rung"),
					Max <= FAIBMovementPolicy::StrafeLegSecondsMax(Ladder[Index - 1]));
			}
		}

		TestEqual(TEXT("Novice legs are the longest"),
			FAIBMovementPolicy::StrafeLegSecondsMax(EAIBCompetence::Novice), 2.00f, 0.0001f);
		TestEqual(TEXT("Expert legs are the shortest"),
			FAIBMovementPolicy::StrafeLegSecondsMin(EAIBCompetence::Expert), 0.35f, 0.0001f);
	});

	It("gates the juke as a CAPABILITY — below Skilled it is exactly 0.0, not a small number", [this]()
	{
		// The roadmap's Halo rule: levels gate capabilities, not just numbers. A tuning
		// pass may make a Novice strafe MORE; it must not be able to make one juke at all,
		// and the only shape that survives any retune is an exact zero. Tolerance 0.f is
		// the whole point of this assertion — 0.001 here would be a different contract.
		TestEqual(TEXT("Novice cannot juke"),
			FAIBMovementPolicy::JukeChance(EAIBCompetence::Novice), 0.f, 0.f);
		TestEqual(TEXT("Trained cannot juke"),
			FAIBMovementPolicy::JukeChance(EAIBCompetence::Trained), 0.f, 0.f);

		// And above the gate it is a real, growing capability, not a token.
		const float Skilled = FAIBMovementPolicy::JukeChance(EAIBCompetence::Skilled);
		const float Expert = FAIBMovementPolicy::JukeChance(EAIBCompetence::Expert);
		TestTrue(TEXT("Skilled jukes"), Skilled > 0.1f);
		TestTrue(TEXT("Expert jukes more"), Expert > Skilled);
		TestTrue(TEXT("and never certainly — a guaranteed reverse is its own rhythm"),
			Expert < 1.f);
	});

	It("makes the ladder OBSERVABLE — a Novice stands still, an Expert fights sideways", [this]()
	{
		// 20 seeds x 200 decision windows: 4000 samples per rung, so a 0.05 rate is
		// measured to about +/-0.004 and the bands below are nowhere near the noise.
		int32 NoviceSamples = 0;
		int32 ExpertSamples = 0;
		const float NoviceRate = StrafeRate(EAIBCompetence::Novice, 20, 200, NoviceSamples);
		const float ExpertRate = StrafeRate(EAIBCompetence::Expert, 20, 200, ExpertSamples);

		// Non-vacuity first: a rate over an empty loop passes every band there is.
		TestEqual(TEXT("Novice windows actually ran"), NoviceSamples, 4000);
		TestEqual(TEXT("Expert windows actually ran"), ExpertSamples, 4000);

		TestTrue(TEXT("a Novice strafes in at most 1 window in 10"), NoviceRate <= 0.10f);
		TestTrue(TEXT("...but is not a statue: it does move sometimes"), NoviceRate > 0.0f);
		TestTrue(TEXT("an Expert strafes in at least 85% of windows"), ExpertRate >= 0.85f);

		// The middle rungs sit between them, in order — the ladder is a ramp, not a step.
		int32 TrainedSamples = 0;
		int32 SkilledSamples = 0;
		const float TrainedRate = StrafeRate(EAIBCompetence::Trained, 20, 200, TrainedSamples);
		const float SkilledRate = StrafeRate(EAIBCompetence::Skilled, 20, 200, SkilledSamples);
		TestEqual(TEXT("Trained windows actually ran"), TrainedSamples, 4000);
		TestEqual(TEXT("Skilled windows actually ran"), SkilledSamples, 4000);
		TestTrue(TEXT("Novice < Trained"), NoviceRate < TrainedRate);
		TestTrue(TEXT("Trained < Skilled"), TrainedRate < SkilledRate);
		TestTrue(TEXT("Skilled < Expert"), SkilledRate < ExpertRate);
	});

	It("holds the intent between decision points — no per-tick reroll, and no draw consumed", [this]()
	{
		FRandomStream TickedRng;
		TickedRng.Initialize(20260826);
		FAIBMovementState Ticked;

		const EAIBStrafeIntent Settled =
			FAIBMovementPolicy::StepStrafe(Ticked, EAIBCompetence::Expert, TickedRng, 0.0);
		const double Deadline = Ticked.NextDecisionAtSeconds;
		TestTrue(TEXT("a leg was drawn and it lies ahead"), Deadline > 0.0);

		// Mid-leg ticks. Expert legs are at least 0.35s, so both of these are inside.
		const EAIBStrafeIntent AtTenth =
			FAIBMovementPolicy::StepStrafe(Ticked, EAIBCompetence::Expert, TickedRng, 0.10);
		const EAIBStrafeIntent AtFifth =
			FAIBMovementPolicy::StepStrafe(Ticked, EAIBCompetence::Expert, TickedRng, 0.20);
		TestIntent(TEXT("stable at 0.10"), AtTenth, Settled);
		TestIntent(TEXT("stable at 0.20"), AtFifth, Settled);
		TestEqual(TEXT("and the deadline did not move"),
			static_cast<float>(Ticked.NextDecisionAtSeconds), static_cast<float>(Deadline), 0.f);

		// The stronger claim: those ticks consumed NO DRAW. An identically seeded run that
		// skips them must land on exactly the same next decision — if the mid-leg calls had
		// rolled anything, the two streams would be out of phase and diverge here. (This is
		// the indirect read of stream state; FRandomStream exposes no draw counter.)
		FRandomStream QuietRng;
		QuietRng.Initialize(20260826);
		FAIBMovementState Quiet;
		FAIBMovementPolicy::StepStrafe(Quiet, EAIBCompetence::Expert, QuietRng, 0.0);

		const EAIBStrafeIntent TickedNext =
			FAIBMovementPolicy::StepStrafe(Ticked, EAIBCompetence::Expert, TickedRng, 100.0);
		const EAIBStrafeIntent QuietNext =
			FAIBMovementPolicy::StepStrafe(Quiet, EAIBCompetence::Expert, QuietRng, 100.0);
		TestIntent(TEXT("the ticked stream is not ahead of the quiet one"), TickedNext, QuietNext);
		TestEqual(TEXT("...and drew the same next leg"),
			static_cast<float>(Ticked.NextDecisionAtSeconds),
			static_cast<float>(Quiet.NextDecisionAtSeconds), 0.0001f);
	});

	It("biases the next leg to REVERSE only from Skilled up — below it the direction is a coin", [this]()
	{
		// 40 seeds x 300 windows. Trained's opportunity rate is 0.40^2, so this still
		// leaves ~1900 adjacent strafing pairs — enough to separate 0.50 from 0.625.
		int32 TrainedOpportunities = 0;
		int32 SkilledOpportunities = 0;
		int32 ExpertOpportunities = 0;
		const float TrainedReversal = ReversalRate(EAIBCompetence::Trained, 40, 300, TrainedOpportunities);
		const float SkilledReversal = ReversalRate(EAIBCompetence::Skilled, 40, 300, SkilledOpportunities);
		const float ExpertReversal = ReversalRate(EAIBCompetence::Expert, 40, 300, ExpertOpportunities);

		// Non-vacuity: the harness must actually have produced reversal opportunities.
		TestTrue(TEXT("Trained had real opportunities"), TrainedOpportunities > 500);
		TestTrue(TEXT("Skilled had real opportunities"), SkilledOpportunities > 500);
		TestTrue(TEXT("Expert had real opportunities"), ExpertOpportunities > 500);

		// Below the gate the direction carries NO memory of the last leg: an even coin,
		// nothing pushing it either way. Both bands matter — too high would be a leaked
		// juke, too low would be a stickiness the contract does not have.
		TestTrue(TEXT("Trained does not reverse more often than a coin"), TrainedReversal <= 0.58f);
		TestTrue(TEXT("Trained does not stick to one side either"), TrainedReversal >= 0.42f);

		// Above it, the same harness shows the bias — and it grows with the rung.
		TestTrue(TEXT("Skilled reverses MORE than the coin"), SkilledReversal > TrainedReversal + 0.05f);
		TestTrue(TEXT("Expert reverses more still"), ExpertReversal > SkilledReversal + 0.05f);
		TestTrue(TEXT("an Expert reverses at least 2 opportunities in 3"), ExpertReversal >= 0.68f);
	});

	It("decides on the first step of a life, and draws every leg inside its own bounds", [this]()
	{
		// A fresh per-life state at a match clock deep into the round: the 0.0 deadline is
		// behind any clock, so the first step decides instead of holding the default Hold
		// until the clock wraps. (The respawn case: state dies with the body, the world's
		// clock does not.)
		FRandomStream Rng;
		Rng.Initialize(7);
		FAIBMovementState State;

		const double Start = 5000.0;
		FAIBMovementPolicy::StepStrafe(State, EAIBCompetence::Skilled, Rng, Start);

		const float Leg = static_cast<float>(State.NextDecisionAtSeconds - Start);
		TestTrue(TEXT("the first step decided and armed a leg"), Leg > 0.f);
		TestTrue(TEXT("the leg is at least Min"),
			Leg >= FAIBMovementPolicy::StrafeLegSecondsMin(EAIBCompetence::Skilled));
		TestTrue(TEXT("the leg is at most Max"),
			Leg <= FAIBMovementPolicy::StrafeLegSecondsMax(EAIBCompetence::Skilled));

		// Every later leg lands in the band too — including the ones after a Hold window,
		// which are timed exactly like a strafe (a hold that re-asked every tick would
		// stutter the instant its odds came up).
		double Now = State.NextDecisionAtSeconds;
		for (int32 Step = 0; Step < 50; ++Step)
		{
			const double Previous = Now;
			FAIBMovementPolicy::StepStrafe(State, EAIBCompetence::Skilled, Rng, Now);
			const float Drawn = static_cast<float>(State.NextDecisionAtSeconds - Previous);
			TestTrue(TEXT("every leg lands inside the band"),
				Drawn >= FAIBMovementPolicy::StrafeLegSecondsMin(EAIBCompetence::Skilled)
				&& Drawn <= FAIBMovementPolicy::StrafeLegSecondsMax(EAIBCompetence::Skilled));
			Now = State.NextDecisionAtSeconds;
		}
	});

	It("repeats exactly under its seed, and drifts to neither side", [this]()
	{
		// Determinism is the debugging contract: a reported fight can be replayed. It is
		// also why the conventions block forbids a shared stream — same seed, same bot.
		TArray<EAIBStrafeIntent> First;
		TArray<EAIBStrafeIntent> Second;
		RunWindows(EAIBCompetence::Expert, 99, 60, First);
		RunWindows(EAIBCompetence::Expert, 99, 60, Second);

		TestEqual(TEXT("both runs produced the full window count"), First.Num(), 60);
		TestEqual(TEXT("the two runs are the same length"), Second.Num(), First.Num());
		int32 Divergences = 0;
		for (int32 Index = 0; Index < First.Num(); ++Index)
		{
			Divergences += (First[Index] != Second[Index]) ? 1 : 0;
		}
		TestEqual(TEXT("same seed, same fight"), Divergences, 0);

		// And no side bias: an even pick that leaned would walk the bot off the arena over
		// a match. 20 seeds x 200 windows at Expert odds is ~3800 directed legs.
		int32 Left = 0;
		int32 Right = 0;
		TArray<EAIBStrafeIntent> Intents;
		for (int32 Seed = 1; Seed <= 20; ++Seed)
		{
			RunWindows(EAIBCompetence::Expert, Seed, 200, Intents);
			for (const EAIBStrafeIntent Intent : Intents)
			{
				Left += (Intent == EAIBStrafeIntent::Left) ? 1 : 0;
				Right += (Intent == EAIBStrafeIntent::Right) ? 1 : 0;
			}
		}
		TestTrue(TEXT("the sweep produced a real sample"), Left + Right > 3000);
		const float Imbalance = static_cast<float>(FMath::Abs(Left - Right))
			/ static_cast<float>(Left + Right);
		TestTrue(TEXT("neither side is favoured"), Imbalance < 0.10f);
	});

	It("marks a REVERSING leg as a juke, and a hold or a fresh pick as not — the hop's trigger", [this]()
	{
		// THE DEFENSIVE HOP rides this flag (AIBStateTreeTasks: a defending bot jumps on the
		// leg that reverses). Getting it wrong in either direction is visible: a stale true
		// makes the bot hop on every leg and read as a pogo stick, a stuck false makes the
		// evasion this was asked for silently never happen.
		//
		// Expert, because JukeChance is 0 below Skilled by design — that is the capability
		// gate the hop inherits rather than re-declaring (R28).
		FAIBMovementState State;
		FRandomStream Rng(20260828);
		const EAIBCompetence Level = EAIBCompetence::Expert;

		bool bSawJukeTrue = false;
		bool bSawJukeFalse = false;
		double Now = 0.0;
		for (int32 Step = 0; Step < 400; ++Step)
		{
			Now = State.NextDecisionAtSeconds;
			const EAIBStrafeIntent Previous = State.Current;
			FAIBMovementPolicy::StepStrafe(State, Level, Rng, Now);

			if (State.Current == EAIBStrafeIntent::Hold)
			{
				// A stand has nothing to reverse off, so it can never be a juke.
				TestFalse(TEXT("a hold leg is never a juke"), State.bLastLegWasJuke);
				bSawJukeFalse = true;
				continue;
			}
			if (State.bLastLegWasJuke)
			{
				// The flag's whole meaning: this leg REVERSED the one before it.
				TestTrue(TEXT("a juke leg has a direction to reverse"), Previous != EAIBStrafeIntent::Hold);
				TestTrue(TEXT("a juke leg actually reversed"), State.Current != Previous);
				bSawJukeTrue = true;
			}
		}
		TestTrue(TEXT("the sweep saw at least one juke"), bSawJukeTrue);
		TestTrue(TEXT("and at least one non-juke"), bSawJukeFalse);
	});

	It("never lets a Novice juke — so the defensive hop cannot reach the low tiers", [this]()
	{
		// The gate the hop leans on, pinned from the hop's side. If JukeChance ever gained a
		// floor above zero this would go red here rather than as a Novice bouncing in a
		// firefight, which is the kind of tier bleed R28 exists to stop.
		FAIBMovementState State;
		FRandomStream Rng(99);
		for (int32 Step = 0; Step < 300; ++Step)
		{
			FAIBMovementPolicy::StepStrafe(State, EAIBCompetence::Novice, Rng, State.NextDecisionAtSeconds);
			TestFalse(TEXT("a Novice leg is never a juke"), State.bLastLegWasJuke);
		}
	});


	It("lets EVERY tier hop, and still climbs — the gate that must not silence the behaviour", [this]()
	{
		// THE REGRESSION THIS EXISTS FOR (28 Aug). The hop first rode JukeChance so it would
		// inherit a tier gate for free. JukeChance is 0.00 below Skilled, so the evasive jump
		// was structurally impossible at Marine — the tier actually played — and a full match
		// measured 9 defend stand-downs and exactly 0 hops. Capability-shaped must not mean
		// capability-SILENCED: R28 asks that tiers differ, not that a behaviour vanish.
		const float Novice  = FAIBMovementPolicy::HopChance(EAIBCompetence::Novice);
		const float Trained = FAIBMovementPolicy::HopChance(EAIBCompetence::Trained);
		const float Skilled = FAIBMovementPolicy::HopChance(EAIBCompetence::Skilled);
		const float Expert  = FAIBMovementPolicy::HopChance(EAIBCompetence::Expert);

		TestTrue(TEXT("a Novice can hop at all"), Novice > 0.f);
		TestTrue(TEXT("and a Trained bot — the Marine rung — can hop"), Trained > 0.f);

		// Monotone, like every other rung on this ladder: no skill ever decreases.
		TestTrue(TEXT("Trained >= Novice"), Trained >= Novice);
		TestTrue(TEXT("Skilled >= Trained"), Skilled >= Trained);
		TestTrue(TEXT("Expert >= Skilled"), Expert >= Skilled);
		TestTrue(TEXT("and the ladder actually climbs"), Expert > Novice);

		// A chance, not a certainty — a bot that hopped every leg would be a pogo stick.
		TestTrue(TEXT("never a certainty"), Expert < 1.f);
	});


	It("lets every tier dash, and still climbs — the same gate the hop needed", [this]()
	{
		// The hop shipped tied to JukeChance, which is 0.00 below Skilled, so the behaviour was
		// structurally impossible at Marine — the tier actually played — and measured ZERO in a
		// full match. DashChance is its own lever for the same reason, and this row is what
		// stops it drifting back to a gate that silences it.
		const float Novice  = FAIBMovementPolicy::DashChance(EAIBCompetence::Novice);
		const float Trained = FAIBMovementPolicy::DashChance(EAIBCompetence::Trained);
		const float Skilled = FAIBMovementPolicy::DashChance(EAIBCompetence::Skilled);
		const float Expert  = FAIBMovementPolicy::DashChance(EAIBCompetence::Expert);

		TestTrue(TEXT("a Novice can dash at all"), Novice > 0.f);
		TestTrue(TEXT("and Trained — the Marine rung — can dash"), Trained > 0.f);
		TestTrue(TEXT("Trained >= Novice"), Trained >= Novice);
		TestTrue(TEXT("Skilled >= Trained"), Skilled >= Trained);
		TestTrue(TEXT("Expert >= Skilled"), Expert >= Skilled);
		TestTrue(TEXT("the ladder climbs"), Expert > Novice);

		// A CHANCE, and a modest one: the throttle bounds the rate, but a roll near 1 would
		// spend the dash the instant it came off cooldown, every time, forever — which reads
		// as a twitch rather than a decision.
		TestTrue(TEXT("never a certainty"), Expert < 0.75f);
	});

}

#endif // WITH_DEV_AUTOMATION_TESTS
