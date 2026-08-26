#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTypes.h"
#include "Skills/AIBMeleePolicy.h"

/**
 * The melee policy, proven headless — no world, no engine objects: a state struct, floats,
 * and time as numbers. The properties pinned here are the ones that read as broken when
 * they break: an instantly lethal point-blank bot (the delay), a bot that banks credit for
 * a knife across separate approaches (the continuous-range reset), and a bot that swings
 * at something it cannot see or cannot range (the unknown paths). Deterministic by
 * construction — this policy draws no randomness, because the delay IS its humaniser.
 */
BEGIN_DEFINE_SPEC(FAIBMeleePolicySpec, "AIBot.Sim.MeleePolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** The four rungs in ladder order — every per-level and monotone claim walks this. */
	TArray<EAIBCompetence> Ladder() const
	{
		return TArray<EAIBCompetence>{ EAIBCompetence::Novice, EAIBCompetence::Trained,
			EAIBCompetence::Skilled, EAIBCompetence::Expert };
	}

	/** Rung names for failure messages: a spec that fails without naming the level costs
	 *  the next session more than the helper costs this one. */
	FString RungName(EAIBCompetence Level) const
	{
		switch (Level)
		{
		case EAIBCompetence::Novice:  return TEXT("Novice");
		case EAIBCompetence::Trained: return TEXT("Trained");
		case EAIBCompetence::Skilled: return TEXT("Skilled");
		case EAIBCompetence::Expert:  return TEXT("Expert");
		default:                      return TEXT("<unknown rung>");
		}
	}

	/** A comfortably-inside-range distance for a rung — half its commit range. */
	float WellInside(EAIBCompetence Level) const
	{
		return FAIBMeleePolicy::CommitRangeUU(Level) * 0.5f;
	}

END_DEFINE_SPEC(FAIBMeleePolicySpec)

void FAIBMeleePolicySpec::Define()
{
	It("waits out the recognition delay before committing — point-blank is not instant", [this]()
	{
		// The whole point of the humaniser: the range being true is not the swing. Every
		// rung must refuse mid-delay and commit AT the edge (>=, not >), so the tuning
		// number means what it says.
		for (const EAIBCompetence Level : Ladder())
		{
			const double Start = 100.0;
			const double Delay = static_cast<double>(FAIBMeleePolicy::RecognitionDelaySeconds(Level));
			const float Dist = WellInside(Level);

			FAIBMeleeState State;
			const FString Rung = RungName(Level);

			TestFalse(*FString::Printf(TEXT("%s: the first tick in range never swings"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, Dist, true, Level, Start));
			TestTrue(*FString::Printf(TEXT("%s: the clock stamped on entry"), *Rung),
				State.InRangeSinceSeconds >= 0.0);

			TestFalse(*FString::Printf(TEXT("%s: half the delay is still too early"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, Dist, true, Level, Start + Delay * 0.5));

			TestTrue(*FString::Printf(TEXT("%s: commits AT the delay's edge"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, Dist, true, Level, Start + Delay));
			TestTrue(*FString::Printf(TEXT("%s: and stays committed after it"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, Dist, true, Level, Start + Delay + 1.0));
		}
	});

	It("commits sooner at Expert than at Novice on one identical timeline", [this]()
	{
		// The same fight, the same clock, two rungs: at the Expert's delay the Expert is
		// already swinging and the Novice is still reading. If this ever passes for both,
		// the ladder has stopped meaning anything.
		const double Start = 100.0;
		const double ExpertDelay =
			static_cast<double>(FAIBMeleePolicy::RecognitionDelaySeconds(EAIBCompetence::Expert));

		// Inside BOTH commit ranges, so only the delay can separate them.
		const float Dist = FMath::Min(WellInside(EAIBCompetence::Novice), WellInside(EAIBCompetence::Expert));

		FAIBMeleeState NoviceState;
		FAIBMeleeState ExpertState;
		FAIBMeleePolicy::ShouldMelee(NoviceState, Dist, true, EAIBCompetence::Novice, Start);
		FAIBMeleePolicy::ShouldMelee(ExpertState, Dist, true, EAIBCompetence::Expert, Start);

		const double Moment = Start + ExpertDelay;
		TestTrue(TEXT("Expert has committed"),
			FAIBMeleePolicy::ShouldMelee(ExpertState, Dist, true, EAIBCompetence::Expert, Moment));
		TestFalse(TEXT("Novice, at the same instant of the same fight, has not"),
			FAIBMeleePolicy::ShouldMelee(NoviceState, Dist, true, EAIBCompetence::Novice, Moment));
	});

	It("restarts the clock on ANY break in range — hopping never accumulates", [this]()
	{
		// The reset law. A juking target that crosses the line, leaves, and comes back
		// must buy the whole delay again; paying it in instalments is how a bot ends up
		// knifing the instant a player brushes past.
		for (const EAIBCompetence Level : Ladder())
		{
			const double Start = 100.0;
			const double Delay = static_cast<double>(FAIBMeleePolicy::RecognitionDelaySeconds(Level));
			const float Inside = WellInside(Level);
			const float Outside = FAIBMeleePolicy::CommitRangeUU(Level) + 50.f;

			FAIBMeleeState State;
			const FString Rung = RungName(Level);

			FAIBMeleePolicy::ShouldMelee(State, Inside, true, Level, Start);
			FAIBMeleePolicy::ShouldMelee(State, Inside, true, Level, Start + Delay * 0.5);

			// One tick out of range, halfway through the delay.
			TestFalse(*FString::Printf(TEXT("%s: out of range answers false"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, Outside, true, Level, Start + Delay * 0.6));
			TestTrue(*FString::Printf(TEXT("%s: and the clock was cleared, not paused"), *Rung),
				State.InRangeSinceSeconds < 0.0);

			// Back in range, at what WOULD have been the original commit time.
			TestFalse(*FString::Printf(TEXT("%s: the original commit time no longer commits"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, Inside, true, Level, Start + Delay));

			// The restarted clock pays out a full delay after the RE-entry, not before.
			const double Reentry = Start + Delay;
			TestFalse(*FString::Printf(TEXT("%s: still early on the restarted clock"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, Inside, true, Level, Reentry + Delay * 0.5));
			TestTrue(*FString::Printf(TEXT("%s: commits a full delay after RE-entry"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, Inside, true, Level, Reentry + Delay));
		}
	});

	It("answers false and resets on an UNKNOWN distance, however long the fight ran", [this]()
	{
		// The facts convention: <0 is unknowable, not near. An unknown range must never
		// read as a range of zero, and it must break the continuity like any other miss.
		for (const EAIBCompetence Level : Ladder())
		{
			const double Start = 100.0;
			const double Delay = static_cast<double>(FAIBMeleePolicy::RecognitionDelaySeconds(Level));
			const float Inside = WellInside(Level);

			FAIBMeleeState State;
			const FString Rung = RungName(Level);

			FAIBMeleePolicy::ShouldMelee(State, Inside, true, Level, Start);
			TestFalse(*FString::Printf(TEXT("%s: unknown distance never commits"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, -1.f, true, Level, Start + Delay + 5.0));
			TestTrue(*FString::Printf(TEXT("%s: unknown distance cleared the clock"), *Rung),
				State.InRangeSinceSeconds < 0.0);

			// And an unknown distance held for a whole delay still commits to nothing.
			FAIBMeleeState Fresh;
			FAIBMeleePolicy::ShouldMelee(Fresh, -1.f, true, Level, Start);
			TestFalse(*FString::Printf(TEXT("%s: an unknown range accrues nothing"), *Rung),
				FAIBMeleePolicy::ShouldMelee(Fresh, -1.f, true, Level, Start + Delay + 5.0));
		}
	});

	It("answers false and resets for an INVISIBLE target standing at zero distance", [this]()
	{
		// Sight is not optional. A target the sensorium has not matured is a target the
		// policy cannot swing at, even when the range says it is on top of the bot —
		// otherwise the knife becomes a wallhack at arm's length (FAIRPLAY F3).
		for (const EAIBCompetence Level : Ladder())
		{
			const double Start = 100.0;
			const double Delay = static_cast<double>(FAIBMeleePolicy::RecognitionDelaySeconds(Level));

			FAIBMeleeState State;
			const FString Rung = RungName(Level);

			// A committed melee, then sight is lost with the target still touching.
			FAIBMeleePolicy::ShouldMelee(State, 0.f, true, Level, Start);
			TestTrue(*FString::Printf(TEXT("%s: visible at zero commits after the delay"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, 0.f, true, Level, Start + Delay));

			TestFalse(*FString::Printf(TEXT("%s: sight lost drops the commit at once"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, 0.f, false, Level, Start + Delay + 0.1));
			TestTrue(*FString::Printf(TEXT("%s: and the clock was cleared"), *Rung),
				State.InRangeSinceSeconds < 0.0);

			// Never-visible, held at zero distance for far longer than the delay.
			FAIBMeleeState Blind;
			FAIBMeleePolicy::ShouldMelee(Blind, 0.f, false, Level, Start);
			TestFalse(*FString::Printf(TEXT("%s: an unseen target never accrues"), *Rung),
				FAIBMeleePolicy::ShouldMelee(Blind, 0.f, false, Level, Start + Delay + 5.0));
		}
	});

	It("walks the ladder monotone: range up every rung, recognition delay down every rung", [this]()
	{
		// The ladder must read in one direction or the tier vector stops meaning
		// "better". Asserted across all four rungs, pairwise, so a single retuned number
		// cannot quietly invert one step.
		const TArray<EAIBCompetence> Rungs = Ladder();
		TestEqual(TEXT("four rungs"), Rungs.Num(), 4);

		for (int32 Index = 1; Index < Rungs.Num(); ++Index)
		{
			const EAIBCompetence Lower = Rungs[Index - 1];
			const EAIBCompetence Upper = Rungs[Index];
			const FString Step = FString::Printf(TEXT("%s -> %s"), *RungName(Lower), *RungName(Upper));

			TestTrue(*FString::Printf(TEXT("%s: commit range increases"), *Step),
				FAIBMeleePolicy::CommitRangeUU(Upper) > FAIBMeleePolicy::CommitRangeUU(Lower));
			TestTrue(*FString::Printf(TEXT("%s: recognition delay decreases"), *Step),
				FAIBMeleePolicy::RecognitionDelaySeconds(Upper) < FAIBMeleePolicy::RecognitionDelaySeconds(Lower));
		}
	});

	It("keeps every rung's delay at or above the module reaction floor — F1 is not a tier knob", [this]()
	{
		// FAIRPLAY F1 binds this ladder like every other latency in the module: the
		// fastest rung may sit ON the floor, never under it. A retune that dips below is
		// a high finding, and this is where it gets caught.
		for (const EAIBCompetence Level : Ladder())
		{
			TestTrue(*FString::Printf(TEXT("%s: delay >= the reaction floor"), *RungName(Level)),
				FAIBMeleePolicy::RecognitionDelaySeconds(Level) >= AIB::MinReactionSeconds);
			TestTrue(*FString::Printf(TEXT("%s: commit range is a positive reach"), *RungName(Level)),
				FAIBMeleePolicy::CommitRangeUU(Level) > 0.f);
		}
	});

	It("commits AT the commit range's edge and refuses one unit beyond it", [this]()
	{
		// The boundary, pinned so a later >/>= edit is visible: the range is inclusive,
		// and a target a single unit further away is out — with the clock cleared.
		for (const EAIBCompetence Level : Ladder())
		{
			const double Start = 100.0;
			const double Delay = static_cast<double>(FAIBMeleePolicy::RecognitionDelaySeconds(Level));
			const float Edge = FAIBMeleePolicy::CommitRangeUU(Level);

			FAIBMeleeState State;
			const FString Rung = RungName(Level);

			FAIBMeleePolicy::ShouldMelee(State, Edge, true, Level, Start);
			TestTrue(*FString::Printf(TEXT("%s: the edge itself is inside"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, Edge, true, Level, Start + Delay));

			TestFalse(*FString::Printf(TEXT("%s: one unit beyond is out"), *Rung),
				FAIBMeleePolicy::ShouldMelee(State, Edge + 1.f, true, Level, Start + Delay));
			TestTrue(*FString::Printf(TEXT("%s: and the step out cleared the clock"), *Rung),
				State.InRangeSinceSeconds < 0.0);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
