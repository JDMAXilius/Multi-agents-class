#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTypes.h"
#include "Math/RandomStream.h"
#include "Skills/AIBGrenadePolicy.h"

/**
 * The grenade ladder, proven headless — no world, no handles: a state struct, facts structs,
 * a seeded stream and time as numbers. What this suite pins is the SHAPE of the ladder, not
 * its tuning: that a Novice's inability is a LEVEL and not a number, that each rung sees
 * exactly what it should and nothing above it, that the band is reachable by facts that can
 * actually occur, that the cadence is a cadence, and that an unknown fact never counts as
 * satisfied. FAIRPLAY F3 is pinned too: the finish is read off damage this bot DEALT, never
 * off a target-health number no human could see.
 */
BEGIN_DEFINE_SPEC(FAIBGrenadePolicySpec, "AIBot.Sim.GrenadePolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** Every rung, so a ladder claim is asserted at all four and never at one convenient one. */
	const EAIBCompetence AllLevels[4] = {
		EAIBCompetence::Novice, EAIBCompetence::Trained,
		EAIBCompetence::Skilled, EAIBCompetence::Expert };

	const TCHAR* LevelName(EAIBCompetence Level) const
	{
		switch (Level)
		{
		case EAIBCompetence::Novice:  return TEXT("Novice");
		case EAIBCompetence::Trained: return TEXT("Trained");
		case EAIBCompetence::Skilled: return TEXT("Skilled");
		case EAIBCompetence::Expert:  return TEXT("Expert");
		}
		return TEXT("<unknown rung>");
	}

	/**
	 * TestEqual has no enum overload, and an enum printed through one is not something to
	 * bet a failure message on. int32 casts compare AND print deterministically:
	 * 0 = None, 1 = Opener, 2 = Finisher, 3 = AreaDenial.
	 */
	bool TestCall(const FString& What, EAIBGrenadeCall Actual, EAIBGrenadeCall Expected)
	{
		return TestEqual(*What, static_cast<int32>(Actual), static_cast<int32>(Expected));
	}

	/** One consideration from a fresh life — the cadence has never fired before this call. */
	EAIBGrenadeCall CallOnce(const FAIBFacts& Facts, EAIBCompetence Level, double Now = 10.0) const
	{
		FAIBGrenadeState State;
		FRandomStream Rng(1337);
		return FAIBGrenadePolicy::Consider(State, Facts, Level, Rng, Now);
	}

	/** A visible target, well inside every throwing level's band, grenades in pocket. */
	FAIBFacts OpenerFacts() const
	{
		FAIBFacts Facts;
		Facts.GrenadeCount = 2;
		Facts.bHasTarget = true;
		Facts.bTargetVisible = true;
		Facts.DistToTargetUU = 800.f;
		return Facts;
	}

	/**
	 * The same moment plus landing pressure. Note what is NOT set: bTargetVitalsKnown stays
	 * false and TargetHealthNorm is never touched — F3's amendment says enemy vitals are not
	 * perceivable, so the finish must be readable without them.
	 */
	FAIBFacts FinisherFacts() const
	{
		FAIBFacts Facts = OpenerFacts();
		Facts.bDamageHistoryKnown = true;
		Facts.RecentDamageDealtNorm = 0.7f;
		return Facts;
	}

	/**
	 * Sight just lost. This is the shape the facts builder ACTUALLY produces when a target
	 * is not held: no target flags, and DistToTargetUU unknown (<0) because the builder only
	 * ranges a held target. Denial has to work from here or it works nowhere.
	 */
	FAIBFacts DenialFacts() const
	{
		FAIBFacts Facts;
		Facts.GrenadeCount = 2;
		Facts.bHasMemory = true;
		Facts.LastKnownAgeSeconds = 1.f;
		Facts.MemoryFreshWindowSeconds = AIB::DefaultMemoryFreshSeconds;
		return Facts;
	}

END_DEFINE_SPEC(FAIBGrenadePolicySpec)

void FAIBGrenadePolicySpec::Define()
{
	It("gates blast evasion by LEVEL, not by tuning — a Novice cannot dodge at any value", [this]()
	{
		// THE capability gate. The signature is the proof as much as the values are: it
		// takes a level and nothing else — no facts, no radius, no tier float — so there is
		// no number anywhere in the module that could buy a Novice this ability.
		TestFalse(TEXT("Novice cannot evade a blast"), FAIBGrenadePolicy::CanEvadeBlast(EAIBCompetence::Novice));
		TestTrue(TEXT("Trained can"), FAIBGrenadePolicy::CanEvadeBlast(EAIBCompetence::Trained));
		TestTrue(TEXT("Skilled can"), FAIBGrenadePolicy::CanEvadeBlast(EAIBCompetence::Skilled));
		TestTrue(TEXT("Expert can"), FAIBGrenadePolicy::CanEvadeBlast(EAIBCompetence::Expert));
	});

	It("keeps every throwing band INSIDE the sight envelope, and leaves Novice's empty", [this]()
	{
		// W-REVIEW P2 C3, applied to this policy: a band no reachable fact can land inside
		// is inert. DistToTargetUU is only known while a target is held, so the reachable
		// range is (0, AIB::EngageFadeEndUU] — every ceiling must sit at or under that.
		for (const EAIBCompetence Level : AllLevels)
		{
			TestTrue(*FString::Printf(TEXT("%s floor clears the full-appetite range"), LevelName(Level)),
				FAIBGrenadePolicy::ThrowBandMinUU(Level) > AIB::EngageFullAppetiteUU);
		}

		// Novice: max BELOW min, so the band cannot contain any distance at all.
		TestTrue(TEXT("Novice's band is empty by construction"),
			FAIBGrenadePolicy::ThrowBandMaxUU(EAIBCompetence::Novice)
				< FAIBGrenadePolicy::ThrowBandMinUU(EAIBCompetence::Novice));

		const float TrainedMax = FAIBGrenadePolicy::ThrowBandMaxUU(EAIBCompetence::Trained);
		const float SkilledMax = FAIBGrenadePolicy::ThrowBandMaxUU(EAIBCompetence::Skilled);
		const float ExpertMax  = FAIBGrenadePolicy::ThrowBandMaxUU(EAIBCompetence::Expert);

		TestTrue(TEXT("Trained's band is non-empty"),
			TrainedMax > FAIBGrenadePolicy::ThrowBandMinUU(EAIBCompetence::Trained));
		TestTrue(TEXT("reach widens Trained -> Skilled"), SkilledMax > TrainedMax);
		TestTrue(TEXT("reach widens Skilled -> Expert"), ExpertMax > SkilledMax);
		// The widest band stops AT the envelope's edge, tied to the named constant.
		TestEqual(TEXT("Expert reaches exactly the envelope"), ExpertMax, AIB::EngageFadeEndUU, 0.001f);
	});

	It("never lets a Novice throw — the opener, the finish and the denial are all a pass", [this]()
	{
		TestCall(TEXT("Novice on a perfect opener"), CallOnce(OpenerFacts(), EAIBCompetence::Novice),
			EAIBGrenadeCall::None);
		TestCall(TEXT("Novice on a perfect finish"), CallOnce(FinisherFacts(), EAIBCompetence::Novice),
			EAIBGrenadeCall::None);
		TestCall(TEXT("Novice on a perfect denial"), CallOnce(DenialFacts(), EAIBCompetence::Novice),
			EAIBGrenadeCall::None);

		// And not merely this instant: a Novice that never stamps a cadence must still be
		// silent on every later look, not accumulating one.
		FAIBGrenadeState State;
		FRandomStream Rng(4);
		const FAIBFacts Facts = FinisherFacts();
		for (int32 Look = 0; Look < 5; ++Look)
		{
			TestCall(FString::Printf(TEXT("Novice look %d"), Look),
				FAIBGrenadePolicy::Consider(State, Facts, EAIBCompetence::Novice, Rng, 10.0 + Look),
				EAIBGrenadeCall::None);
		}
	});

	It("recognises the OPENER from Trained up, and only inside that level's band", [this]()
	{
		const FAIBFacts Facts = OpenerFacts(); // 800uu: inside all three throwing bands
		TestCall(TEXT("Novice"),  CallOnce(Facts, EAIBCompetence::Novice),  EAIBGrenadeCall::None);
		TestCall(TEXT("Trained"), CallOnce(Facts, EAIBCompetence::Trained), EAIBGrenadeCall::Opener);
		TestCall(TEXT("Skilled"), CallOnce(Facts, EAIBCompetence::Skilled), EAIBGrenadeCall::Opener);
		TestCall(TEXT("Expert"),  CallOnce(Facts, EAIBCompetence::Expert),  EAIBGrenadeCall::Opener);

		// Point blank is a pass at every level — the floor is the bot not blasting itself.
		FAIBFacts TooClose = Facts;
		TooClose.DistToTargetUU = 300.f;
		for (const EAIBCompetence Level : AllLevels)
		{
			TestCall(FString::Printf(TEXT("%s at 300uu"), LevelName(Level)),
				CallOnce(TooClose, Level), EAIBGrenadeCall::None);
		}

		// And reach is itself a rung: the same far target is out of reach for Trained,
		// in reach for Skilled, and further out only Expert still throws.
		FAIBFacts Far = Facts;
		Far.DistToTargetUU = 1200.f;
		TestCall(TEXT("1200uu is past Trained's reach"), CallOnce(Far, EAIBCompetence::Trained), EAIBGrenadeCall::None);
		TestCall(TEXT("1200uu is inside Skilled's"),     CallOnce(Far, EAIBCompetence::Skilled), EAIBGrenadeCall::Opener);

		Far.DistToTargetUU = 1400.f;
		TestCall(TEXT("1400uu is past Skilled's reach"), CallOnce(Far, EAIBCompetence::Skilled), EAIBGrenadeCall::None);
		TestCall(TEXT("1400uu is still inside Expert's"), CallOnce(Far, EAIBCompetence::Expert), EAIBGrenadeCall::Opener);
	});

	It("reads the FINISHER at Skilled — Trained sees the same facts as a plain opener", [this]()
	{
		// The progression that matters: identical facts, four rungs, three different reads.
		// A Trained bot is not blind here, it is LESS PERCEPTIVE — it recognises the
		// opening and misses that the pressure is already landing.
		const FAIBFacts Facts = FinisherFacts();
		TestCall(TEXT("Novice"),  CallOnce(Facts, EAIBCompetence::Novice),  EAIBGrenadeCall::None);
		TestCall(TEXT("Trained"), CallOnce(Facts, EAIBCompetence::Trained), EAIBGrenadeCall::Opener);
		TestCall(TEXT("Skilled"), CallOnce(Facts, EAIBCompetence::Skilled), EAIBGrenadeCall::Finisher);
		TestCall(TEXT("Expert"),  CallOnce(Facts, EAIBCompetence::Expert),  EAIBGrenadeCall::Finisher);

		// Below the bar the pressure is not a finish — the opener is the honest lesser read.
		FAIBFacts Light = Facts;
		Light.RecentDamageDealtNorm = 0.2f;
		TestCall(TEXT("light pressure is only an opening"),
			CallOnce(Light, EAIBCompetence::Expert), EAIBGrenadeCall::Opener);
	});

	It("calls AREA DENIAL only at Expert, on the fact shape a lost target actually leaves", [this]()
	{
		const FAIBFacts Facts = DenialFacts();
		TestCall(TEXT("Novice"),  CallOnce(Facts, EAIBCompetence::Novice),  EAIBGrenadeCall::None);
		TestCall(TEXT("Trained"), CallOnce(Facts, EAIBCompetence::Trained), EAIBGrenadeCall::None);
		TestCall(TEXT("Skilled"), CallOnce(Facts, EAIBCompetence::Skilled), EAIBGrenadeCall::None);
		TestCall(TEXT("Expert"),  CallOnce(Facts, EAIBCompetence::Expert),  EAIBGrenadeCall::AreaDenial);

		// THE RULING THIS PINS (the contract left it open): DenialFacts carries an UNKNOWN
		// distance, because the facts builder only ranges a HELD target. Had denial demanded
		// a known in-band distance it could never fire from any producible fact set — an
		// inert Expert capability, the same defect class as a band outside the envelope.
		TestTrue(TEXT("the denial fact set really does carry an unknown distance"),
			Facts.DistToTargetUU < 0.f);
		// It is also NOT gated on bTargetFactsFromMemory: that flag is a held-belief marker
		// the builder sets only while a target is still held, so it is false in exactly the
		// situation denial exists for. Requiring it would have been the inert version.
		TestFalse(TEXT("no held-belief marker here"), Facts.bTargetFactsFromMemory);
	});

	It("refuses denial on a stale or unknowable memory — freshness is the whole condition", [this]()
	{
		FAIBFacts Stale = DenialFacts();
		Stale.LastKnownAgeSeconds = 12.f; // past half the tier window: they are long gone
		TestCall(TEXT("a cold trail is not a denial"),
			CallOnce(Stale, EAIBCompetence::Expert), EAIBGrenadeCall::None);

		FAIBFacts NoMemory = DenialFacts();
		NoMemory.bHasMemory = false;
		TestCall(TEXT("no memory, no throw"),
			CallOnce(NoMemory, EAIBCompetence::Expert), EAIBGrenadeCall::None);

		// An unset window is UNKNOWABLE freshness, not fresh freshness — unknown is a state.
		FAIBFacts NoWindow = DenialFacts();
		NoWindow.MemoryFreshWindowSeconds = 0.f;
		TestCall(TEXT("an unknowable window never satisfies"),
			CallOnce(NoWindow, EAIBCompetence::Expert), EAIBGrenadeCall::None);
	});

	It("considers on a cadence — a look inside the window is None and does NOT restamp", [this]()
	{
		// The bug this pins: restamping on a refused look pushes the next glance forward
		// forever at think rate, and the bot never throws again for the rest of its life.
		FAIBGrenadeState State;
		FRandomStream Rng(11);
		const FAIBFacts Facts = OpenerFacts();
		const float Cadence = FAIBGrenadePolicy::ConsiderSeconds(EAIBCompetence::Trained);

		TestCall(TEXT("the first look throws"),
			FAIBGrenadePolicy::Consider(State, Facts, EAIBCompetence::Trained, Rng, 10.0),
			EAIBGrenadeCall::Opener);
		TestEqual(TEXT("stamped one cadence ahead"),
			static_cast<float>(State.NextConsiderAtSeconds), 10.f + Cadence, 0.001f);

		TestCall(TEXT("second look, inside the window"),
			FAIBGrenadePolicy::Consider(State, Facts, EAIBCompetence::Trained, Rng, 10.5),
			EAIBGrenadeCall::None);
		TestCall(TEXT("third look, still inside"),
			FAIBGrenadePolicy::Consider(State, Facts, EAIBCompetence::Trained, Rng, 11.0),
			EAIBGrenadeCall::None);
		TestEqual(TEXT("the stamp advanced exactly ONCE"),
			static_cast<float>(State.NextConsiderAtSeconds), 10.f + Cadence, 0.001f);

		// The behavioural half of the same claim: past the window the next one fires.
		TestCall(TEXT("past the window it considers again"),
			FAIBGrenadePolicy::Consider(State, Facts, EAIBCompetence::Trained, Rng, 10.0 + Cadence + 0.01),
			EAIBGrenadeCall::Opener);

		// The rungs glance at different rates — recognition speed is part of the ladder.
		TestTrue(TEXT("Skilled glances faster than Trained"),
			FAIBGrenadePolicy::ConsiderSeconds(EAIBCompetence::Skilled) < Cadence);
		TestTrue(TEXT("Expert glances faster than Skilled"),
			FAIBGrenadePolicy::ConsiderSeconds(EAIBCompetence::Expert)
				< FAIBGrenadePolicy::ConsiderSeconds(EAIBCompetence::Skilled));
	});

	It("passes on an empty pocket at every level, and spends no cadence doing it", [this]()
	{
		FAIBFacts Empty = FinisherFacts();
		Empty.GrenadeCount = 0;
		for (const EAIBCompetence Level : AllLevels)
		{
			TestCall(FString::Printf(TEXT("%s with no grenades"), LevelName(Level)),
				CallOnce(Empty, Level), EAIBGrenadeCall::None);
		}

		// An empty pocket is not a consideration, so it must not burn the glance: the very
		// next think after a pickup may see the moment.
		FAIBGrenadeState State;
		FRandomStream Rng(21);
		FAIBGrenadePolicy::Consider(State, Empty, EAIBCompetence::Expert, Rng, 10.0);
		TestEqual(TEXT("no cadence spent on an empty pocket"),
			static_cast<float>(State.NextConsiderAtSeconds), 0.f, 0.001f);

		FAIBFacts Restocked = Empty;
		Restocked.GrenadeCount = 1;
		TestCall(TEXT("the pickup is seen at the same instant"),
			FAIBGrenadePolicy::Consider(State, Restocked, EAIBCompetence::Expert, Rng, 10.0),
			EAIBGrenadeCall::Finisher);
	});

	It("never counts an UNKNOWN as satisfied — no history is no finish, no range is no throw", [this]()
	{
		// The module's unknown-is-a-state rule (F-6.10), on this policy's two unknowns.
		FAIBFacts NoHistory = FinisherFacts();
		NoHistory.bDamageHistoryKnown = false; // the number is still 0.7 and still a lie
		TestCall(TEXT("an unread damage history cannot finish"),
			CallOnce(NoHistory, EAIBCompetence::Expert), EAIBGrenadeCall::Opener);

		FAIBFacts NoRange = OpenerFacts();
		NoRange.DistToTargetUU = -1.f;
		for (const EAIBCompetence Level : AllLevels)
		{
			TestCall(FString::Printf(TEXT("%s with an unknown range"), LevelName(Level)),
				CallOnce(NoRange, Level), EAIBGrenadeCall::None);
		}
	});

	It("is deterministic over the facts — the stream's state never changes the call", [this]()
	{
		// The ladder is RECOGNITION, not a per-throw dice roll (F4's shape, applied here):
		// two bots at the same level reading the same moment must reach the same answer, or
		// the level stops meaning anything. The stream is in the signature for a future
		// draw; today nothing consumes it, and this pins that.
		const FAIBFacts Facts = FinisherFacts();

		FAIBGrenadeState StateA;
		FRandomStream RngA(1);
		const EAIBGrenadeCall CallA =
			FAIBGrenadePolicy::Consider(StateA, Facts, EAIBCompetence::Skilled, RngA, 10.0);

		FAIBGrenadeState StateB;
		FRandomStream RngB(987654);
		for (int32 Burn = 0; Burn < 16; ++Burn)
		{
			RngB.FRandRange(0.f, 1.f); // a stream advanced somewhere else entirely
		}
		const EAIBGrenadeCall CallB =
			FAIBGrenadePolicy::Consider(StateB, Facts, EAIBCompetence::Skilled, RngB, 10.0);

		TestCall(TEXT("same facts, same call"), CallB, CallA);
		TestCall(TEXT("and it is the finish"), CallA, EAIBGrenadeCall::Finisher);
		TestEqual(TEXT("both paid the same cadence"),
			static_cast<float>(StateB.NextConsiderAtSeconds),
			static_cast<float>(StateA.NextConsiderAtSeconds), 0.001f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
