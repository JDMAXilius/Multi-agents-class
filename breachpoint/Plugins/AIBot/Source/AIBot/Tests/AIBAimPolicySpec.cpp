#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTypes.h"
#include "Skills/AIBAimPolicy.h"

/**
 * F4, proven headless — no world, no actors, no controller: an eye position, a belief
 * position, a seeded stream, and time as numbers. Every pin below measures the ANGLE
 * between the true aim line (eye -> belief) and the returned one (eye -> aim point),
 * because that angle is the thing the law is written about; the offset length is only
 * how the policy expresses it.
 *
 * What each pin defends: the settle is a decay and not a snap (1); a target switch buys
 * no free re-acquire (2); nothing is re-rolled per tick, so a shot is never a dice roll
 * (3); competence is a real ladder and not a label (4); the error is angular, so distance
 * matters (5); the wander cadence exists (6); the degenerate line does not explode (7);
 * a tracking gap pays the full re-acquire (8); and the residual floor holds on every
 * rung (9) — (8) and (9) are the W-REVIEW P4+5 F-H1/F-H2 fixes, pinned.
 */
BEGIN_DEFINE_SPEC(FAIBAimPolicySpec, "AIBot.Sim.AimPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** The eye, and a belief 1500uu out — a duel range, flat, so the geometry is readable. */
	const FVector Eye = FVector(0.f, 0.f, 180.f);
	const FVector Belief = FVector(1500.f, 0.f, 180.f);

	/** The measured quantity: degrees between the true line and the aimed one. */
	float ErrorAngleDegrees(const FVector& EyeAt, const FVector& BeliefAt, const FVector& AimAt) const
	{
		const FVector True = (BeliefAt - EyeAt).GetSafeNormal();
		const FVector Aimed = (AimAt - EyeAt).GetSafeNormal();
		const float Dot = FMath::Clamp(FVector::DotProduct(True, Aimed), -1.f, 1.f);
		return FMath::RadiansToDegrees(FMath::Acos(Dot));
	}

	/** Every spec seeds its own stream: shared sequences read as coordinated omniscience. */
	FRandomStream Seeded(int32 Seed) const
	{
		FRandomStream Stream;
		Stream.Initialize(Seed);
		return Stream;
	}

	/**
	 * Steps the policy at field cadence (0.1s — one think) from just after From up to
	 * exactly To, returning the LAST aim point. The policy now treats a step gap over
	 * ReacquireGapSeconds as a tracking loss (the corner-peek fix), so a spec that
	 * samples sparsely is describing a bot that stopped tracking — these pins mean
	 * CONTINUOUS tracking, and now they say so. Callers keep the window inside one
	 * wander cadence themselves, as before.
	 */
	FVector PumpTo(FAIBAimState& State, uint32 Id, EAIBCompetence Level,
		FRandomStream& Rng, double From, double To) const
	{
		FVector Last = FVector::ZeroVector;
		for (double T = From + 0.1; T < To; T += 0.1)
		{
			Last = FAIBAimPolicy::StepAimPoint(State, Eye, Belief, Id, Level, Rng, T);
		}
		return FAIBAimPolicy::StepAimPoint(State, Eye, Belief, Id, Level, Rng, To);
	}

END_DEFINE_SPEC(FAIBAimPolicySpec)

void FAIBAimPolicySpec::Define()
{
	It("decays a fresh error to zero over the correction time — the settle IS the law", [this]()
	{
		const EAIBCompetence Level = EAIBCompetence::Trained;
		const float HalfCone = FAIBAimPolicy::ErrorConeDegrees(Level);
		const float Correct = FAIBAimPolicy::CorrectSeconds(Level);
		// The whole window fits inside one wander cadence, so this measures the settle only.
		TestTrue(TEXT("Trained settles before it wanders"),
			Correct < FAIBAimPolicy::RedrawSeconds(Level));

		FAIBAimState State;
		FRandomStream Rng = Seeded(1337);

		const FVector AtDraw = FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 7u, Level, Rng, 0.0);
		const float ErrorAtDraw = ErrorAngleDegrees(Eye, Belief, AtDraw);
		TestTrue(TEXT("a fresh acquire is never a lucky zero"), ErrorAtDraw > 0.f);
		TestTrue(TEXT("...and lands in the OUTER half of the cone"),
			ErrorAtDraw >= HalfCone * 0.5f - 0.001f);
		TestTrue(TEXT("...and never outside it"), ErrorAtDraw <= HalfCone + 0.001f);

		// Three samples inside the settle, tracked continuously: strictly decreasing,
		// never a step.
		const float ErrorEarly = ErrorAngleDegrees(Eye, Belief, PumpTo(State, 7u, Level, Rng, 0.0, 0.4));
		const float ErrorLate = ErrorAngleDegrees(Eye, Belief, PumpTo(State, 7u, Level, Rng, 0.4, 0.8));
		TestTrue(TEXT("mid-settle is strictly below the draw"), ErrorEarly < ErrorAtDraw);
		TestTrue(TEXT("mid-settle is strictly above zero"), ErrorEarly > 0.f);
		TestTrue(TEXT("and it keeps falling"), ErrorLate < ErrorEarly);

		// At the correction time the settle has arrived — at the RESIDUAL, not at zero:
		// the floor is the F-H1 fix, and this pin is what stops it regressing to perfect.
		const FVector Settled = PumpTo(State, 7u, Level, Rng, 0.8, static_cast<double>(Correct));
		TestEqual(TEXT("settled: the residual jitter, exactly — never zero"),
			ErrorAngleDegrees(Eye, Belief, Settled),
			FAIBAimPolicy::ResidualErrorDegrees(Level), 0.01f);
	});

	It("restarts the settle on a target switch — switching costs the whole re-acquire", [this]()
	{
		const EAIBCompetence Level = EAIBCompetence::Trained;
		const float HalfCone = FAIBAimPolicy::ErrorConeDegrees(Level);
		const float Correct = FAIBAimPolicy::CorrectSeconds(Level);

		FAIBAimState State;
		FRandomStream Rng = Seeded(4242);

		FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 1u, Level, Rng, 0.0);
		const FVector OnTarget = PumpTo(State, 1u, Level, Rng, 0.0, static_cast<double>(Correct));
		TestEqual(TEXT("target A is settled to the residual"),
			ErrorAngleDegrees(Eye, Belief, OnTarget),
			FAIBAimPolicy::ResidualErrorDegrees(Level), 0.01f);

		// A new id one tick later — still inside the wander window, so the SWITCH is what
		// fires. No flick: the error jumps back to a full outer-half draw.
		const FVector AfterSwitch = FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 2u, Level, Rng,
			static_cast<double>(Correct) + 0.1);
		const float ErrorAfterSwitch = ErrorAngleDegrees(Eye, Belief, AfterSwitch);
		TestTrue(TEXT("the switch restarts the settle at outer-half strength"),
			ErrorAfterSwitch >= HalfCone * 0.5f - 0.001f);
		TestEqual(TEXT("the clock restarted with it"), static_cast<float>(State.DrawnAtSeconds),
			static_cast<float>(Correct) + 0.1f, 0.001f);
		TestEqual(TEXT("and the new id is the one held"), static_cast<int32>(State.TargetId), 2);
	});

	It("rolls nothing per tick — the same instant returns the same point, and no draw", [this]()
	{
		// THE ANTI-DICE-ROLL PIN. Two asks at one instant, and a second ask inside the
		// wander window, must consume nothing from the stream and answer identically:
		// a shot fired between two ticks cannot have been re-rolled.
		const EAIBCompetence Level = EAIBCompetence::Skilled;
		FAIBAimState State;
		FRandomStream Rng = Seeded(90210);

		const FVector First = FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 3u, Level, Rng, 5.0);
		const int32 SeedAfterDraw = Rng.GetCurrentSeed();

		const FVector Again = FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 3u, Level, Rng, 5.0);
		TestTrue(TEXT("same instant, same aim point"), Again.Equals(First, 0.0001f));
		TestEqual(TEXT("same instant consumed no draw"), Rng.GetCurrentSeed(), SeedAfterDraw);

		// Later, still inside the window: the point MOVES (it is settling) but only by the
		// decay — still no draw.
		const FVector Mid = FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 3u, Level, Rng, 5.2);
		const FVector MidAgain = FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 3u, Level, Rng, 5.2);
		TestTrue(TEXT("inside the window, still deterministic"), MidAgain.Equals(Mid, 0.0001f));
		TestEqual(TEXT("inside the window consumed no draw"), Rng.GetCurrentSeed(), SeedAfterDraw);
		TestTrue(TEXT("but the aim did settle toward the belief"),
			ErrorAngleDegrees(Eye, Belief, Mid) < ErrorAngleDegrees(Eye, Belief, First));
	});

	It("climbs a real ladder — Expert's cone is tighter AND its error smaller on one seed", [this]()
	{
		TestTrue(TEXT("cones tighten with competence"),
			FAIBAimPolicy::ErrorConeDegrees(EAIBCompetence::Expert)
				< FAIBAimPolicy::ErrorConeDegrees(EAIBCompetence::Skilled));
		TestTrue(TEXT("...all the way down"),
			FAIBAimPolicy::ErrorConeDegrees(EAIBCompetence::Skilled)
				< FAIBAimPolicy::ErrorConeDegrees(EAIBCompetence::Trained));
		TestTrue(TEXT("...to the Novice's widest"),
			FAIBAimPolicy::ErrorConeDegrees(EAIBCompetence::Trained)
				< FAIBAimPolicy::ErrorConeDegrees(EAIBCompetence::Novice));
		TestTrue(TEXT("and settles get faster"),
			FAIBAimPolicy::CorrectSeconds(EAIBCompetence::Expert)
				< FAIBAimPolicy::CorrectSeconds(EAIBCompetence::Novice));
		TestTrue(TEXT("and better aims wander LESS often"),
			FAIBAimPolicy::RedrawSeconds(EAIBCompetence::Expert)
				> FAIBAimPolicy::RedrawSeconds(EAIBCompetence::Novice));

		// Same seed, same instant, same geometry: the only difference is the rung.
		FAIBAimState NoviceState;
		FRandomStream NoviceRng = Seeded(77);
		const float NoviceError = ErrorAngleDegrees(Eye, Belief, FAIBAimPolicy::StepAimPoint(
			NoviceState, Eye, Belief, 1u, EAIBCompetence::Novice, NoviceRng, 0.0));

		FAIBAimState ExpertState;
		FRandomStream ExpertRng = Seeded(77);
		const float ExpertError = ErrorAngleDegrees(Eye, Belief, FAIBAimPolicy::StepAimPoint(
			ExpertState, Eye, Belief, 1u, EAIBCompetence::Expert, ExpertRng, 0.0));

		TestTrue(TEXT("the Expert is off by less on the same draw"), ExpertError <= NoviceError);
		TestTrue(TEXT("...and is still off by something — no rung snaps"), ExpertError > 0.f);
	});

	It("carries an ANGULAR error — doubling the range doubles the offset", [this]()
	{
		// The tan model, pinned: a fixed world-space offset would make far targets easy.
		const EAIBCompetence Level = EAIBCompetence::Trained;
		const FVector Near = Eye + FVector(1000.f, 0.f, 0.f);
		const FVector Far = Eye + FVector(2000.f, 0.f, 0.f);

		FAIBAimState NearState;
		FRandomStream NearRng = Seeded(555);
		const FVector NearAim = FAIBAimPolicy::StepAimPoint(NearState, Eye, Near, 1u, Level, NearRng, 0.0);

		FAIBAimState FarState;
		FRandomStream FarRng = Seeded(555);
		const FVector FarAim = FAIBAimPolicy::StepAimPoint(FarState, Eye, Far, 1u, Level, FarRng, 0.0);

		const float NearOffset = (NearAim - Near).Size();
		const float FarOffset = (FarAim - Far).Size();
		TestTrue(TEXT("there is an offset to compare"), NearOffset > 1.f);
		TestEqual(TEXT("twice the range, twice the miss"), FarOffset, NearOffset * 2.f, 0.5f);
		TestEqual(TEXT("and the ANGLE is unchanged"),
			ErrorAngleDegrees(Eye, Far, FarAim), ErrorAngleDegrees(Eye, Near, NearAim), 0.001f);
	});

	It("redraws on the wander cadence — a settled aim does not stay frozen forever", [this]()
	{
		const EAIBCompetence Level = EAIBCompetence::Trained;
		const float HalfCone = FAIBAimPolicy::ErrorConeDegrees(Level);
		const double Redraw = static_cast<double>(FAIBAimPolicy::RedrawSeconds(Level));

		FAIBAimState State;
		FRandomStream Rng = Seeded(31337);
		FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 9u, Level, Rng, 0.0);
		TestEqual(TEXT("the cadence was armed at the draw"),
			static_cast<float>(State.NextRedrawAtSeconds), static_cast<float>(Redraw), 0.001f);

		// Settled and quiet just before the cadence expires.
		const FVector Quiet = PumpTo(State, 9u, Level, Rng, 0.0, Redraw - 0.01);
		TestEqual(TEXT("settled at the residual before the wander"),
			ErrorAngleDegrees(Eye, Belief, Quiet),
			FAIBAimPolicy::ResidualErrorDegrees(Level), 0.01f);

		// Crossing the boundary draws again — same target, so the draw may be anywhere in
		// the cone (that is the wander, not a re-acquire).
		FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 9u, Level, Rng, Redraw);
		TestEqual(TEXT("the wander restamped the draw clock"),
			static_cast<float>(State.DrawnAtSeconds), static_cast<float>(Redraw), 0.001f);
		TestEqual(TEXT("and re-armed the next one"),
			static_cast<float>(State.NextRedrawAtSeconds), static_cast<float>(Redraw * 2.0), 0.001f);
		TestTrue(TEXT("the wander draw stays inside the cone"),
			State.DrawnErrorDegrees >= 0.f && State.DrawnErrorDegrees <= HalfCone + 0.001f);
	});

	It("pays the full re-acquire after a tracking GAP — the corner re-peek is not free", [this]()
	{
		// THE F-H2 PIN. Old behaviour: same id + inside the wander window = no draw at
		// all, so a target ducking behind a pillar and re-peeking was answered with the
		// already-settled (floor) error — a flick with a delay in front of it. The gap
		// rule makes any step pause beyond ReacquireGapSeconds a re-acquisition.
		const EAIBCompetence Level = EAIBCompetence::Trained;
		const float HalfCone = FAIBAimPolicy::ErrorConeDegrees(Level);
		const double Correct = static_cast<double>(FAIBAimPolicy::CorrectSeconds(Level));

		FAIBAimState State;
		FRandomStream Rng = Seeded(2468);
		FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 4u, Level, Rng, 0.0);
		PumpTo(State, 4u, Level, Rng, 0.0, Correct); // settled on target 4

		// The pillar: no steps for 0.6s (twice the gap threshold), SAME id re-appears.
		const FVector RePeek = FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 4u, Level, Rng,
			Correct + 0.6);
		TestTrue(TEXT("the re-peek is answered with a fresh OUTER-HALF draw"),
			ErrorAngleDegrees(Eye, Belief, RePeek) >= HalfCone * 0.5f - 0.001f);
		TestEqual(TEXT("and the settle clock restarted at the re-peek"),
			static_cast<float>(State.DrawnAtSeconds), static_cast<float>(Correct) + 0.6f, 0.001f);
	});

	It("keeps a residual on every rung — below its own outer half, above zero", [this]()
	{
		// The floor's two structural guarantees: a switch draw always starts ABOVE where
		// it will settle (the journey stays visible), and no rung ever reaches perfect.
		const EAIBCompetence Levels[] = { EAIBCompetence::Novice, EAIBCompetence::Trained,
			EAIBCompetence::Skilled, EAIBCompetence::Expert };
		for (EAIBCompetence Level : Levels)
		{
			const float Residual = FAIBAimPolicy::ResidualErrorDegrees(Level);
			TestTrue(TEXT("residual is above zero"), Residual > 0.f);
			TestTrue(TEXT("residual sits below the outer-half draw floor"),
				Residual < FAIBAimPolicy::ErrorConeDegrees(Level) * 0.5f);
		}
		TestTrue(TEXT("and the floor still tightens with competence"),
			FAIBAimPolicy::ResidualErrorDegrees(EAIBCompetence::Expert)
				< FAIBAimPolicy::ResidualErrorDegrees(EAIBCompetence::Novice));
	});

	It("answers the belief point when there is no aim line to be wrong about", [this]()
	{
		// Degenerate input (eye sitting on the belief): no direction exists, so no error
		// can be expressed — and nothing may be drawn or divided by.
		FAIBAimState State;
		FRandomStream Rng = Seeded(8);
		const int32 SeedBefore = Rng.GetCurrentSeed();

		const FVector Aim = FAIBAimPolicy::StepAimPoint(State, Eye, Eye, 5u, EAIBCompetence::Novice, Rng, 1.0);
		TestTrue(TEXT("the belief point, unchanged"), Aim.Equals(Eye, 0.0001f));
		TestEqual(TEXT("and the stream was not touched"), Rng.GetCurrentSeed(), SeedBefore);

		// And the very next real line still reads as a first acquire, not as a stale hold.
		const FVector Recovered = FAIBAimPolicy::StepAimPoint(State, Eye, Belief, 5u,
			EAIBCompetence::Novice, Rng, 1.1);
		TestTrue(TEXT("a real line draws a real error"),
			ErrorAngleDegrees(Eye, Belief, Recovered)
				>= FAIBAimPolicy::ErrorConeDegrees(EAIBCompetence::Novice) * 0.5f - 0.001f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
