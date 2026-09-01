#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTypes.h"
#include "Perception/AIBTargetPolicy.h"

/**
 * WHO AM I FIGHTING, pinned headless.
 *
 * Every pin below is one of the founder's own sentences turned into arithmetic (1 Sep):
 * knowing who is shooting, preferring the closer threat, being persistent, letting a
 * runner go, and never manufacturing knowledge to do any of it. They are written as
 * COMPARISONS rather than against magic numbers, so retuning the weights in AIBTypes.h
 * changes the bots without breaking the promises.
 */
BEGIN_DEFINE_SPEC(FAIBTargetPolicySpec, "AIBot.Sim.TargetPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static constexpr float Window = 16.f;

	static FAIBTargetScoreInput Seen(float DistanceUU, float SinceSeen = 0.f)
	{
		FAIBTargetScoreInput In;
		In.bSightCurrent = true;
		In.DistanceUU = DistanceUU;
		In.SecondsSinceSeen = SinceSeen;
		return In;
	}

	static FAIBTargetScoreInput Remembered(float DistanceUU, float SinceSeen)
	{
		FAIBTargetScoreInput In;
		In.DistanceUU = DistanceUU;
		In.SecondsSinceSeen = SinceSeen;
		return In;
	}

	static FAIBTargetScoreInput Shooter(float DistanceUU, float SinceHit)
	{
		FAIBTargetScoreInput In;
		In.DistanceUU = DistanceUU;
		In.SecondsSinceSeen = SinceHit;
		In.SecondsSinceDamagedMe = SinceHit;
		return In;
	}

END_DEFINE_SPEC(FAIBTargetPolicySpec)

void FAIBTargetPolicySpec::Define()
{
	Describe("who is shooting me", [this]()
	{
		It("becomes the target when the bot has none", [this]()
		{
			// "if you shoot the AI, it doesn't really have a way of knowing that it's
			// been shot... especially if the AI doesn't have a target". One candidate,
			// known only because it hurt us: it is chosen, and it is chosen as a BELIEF
			// (bSightCurrent false), not as a live track.
			const TArray<FAIBTargetScoreInput> C = { Shooter(2500.f, 0.2f) };
			TestEqual(TEXT("the shooter"), FAIBTargetPolicy::Choose(C, INDEX_NONE, Window), 0);
		});

		It("outranks an equally distant enemy that is merely remembered", [this]()
		{
			TestTrue(TEXT("shooting beats remembered"),
				FAIBTargetPolicy::Score(Shooter(2000.f, 0.5f), Window)
				> FAIBTargetPolicy::Score(Remembered(2000.f, 0.5f), Window));
		});

		It("does NOT outrank a visible enemy at knife range", [this]()
		{
			// The counterweight, and the reason the threat term can be weighted above
			// visibility safely: a bot being sniped while someone is stabbing it deals
			// with the knife. Index 0 is the stabber and it is not the incumbent, so
			// this is decided on merit alone.
			const TArray<FAIBTargetScoreInput> C = { Seen(150.f), Shooter(3500.f, 0.1f) };
			TestEqual(TEXT("the one in your face"),
				FAIBTargetPolicy::Choose(C, INDEX_NONE, Window), 0);
		});

		It("fades — an attacker from ten seconds ago is just an old lead", [this]()
		{
			TestTrue(TEXT("threat decays"),
				FAIBTargetPolicy::Score(Shooter(2000.f, 0.f), Window)
				> FAIBTargetPolicy::Score(Shooter(2000.f, 12.f), Window));
		});
	});

	Describe("closer and clearer wins", [this]()
	{
		It("prefers the nearer of two identical enemies", [this]()
		{
			const TArray<FAIBTargetScoreInput> C = { Seen(2800.f), Seen(400.f) };
			TestEqual(TEXT("the near one"), FAIBTargetPolicy::Choose(C, INDEX_NONE, Window), 1);
		});

		It("prefers the one it can see over the one it only remembers", [this]()
		{
			TestTrue(TEXT("sight beats memory at equal range"),
				FAIBTargetPolicy::Score(Seen(1000.f), Window)
				> FAIBTargetPolicy::Score(Remembered(1000.f, 0.f), Window));
		});

		It("scores an unplaceable candidate no proximity — never infinite proximity", [this]()
		{
			// The facts convention: negative is UNKNOWN. Scoring it as distance zero
			// would make the least-known enemy the most urgent one.
			FAIBTargetScoreInput Unknown;
			Unknown.bSightCurrent = true;
			Unknown.DistanceUU = -1.f;
			TestTrue(TEXT("unknown < known-and-close"),
				FAIBTargetPolicy::Score(Unknown, Window) < FAIBTargetPolicy::Score(Seen(0.f), Window));
		});
	});

	Describe("persistence", [this]()
	{
		It("keeps the current target when a challenger is only marginally better", [this]()
		{
			// "I would try to be as persistent as possible, realistically, to kill this
			// target." Two enemies at almost the same range is precisely where a naive
			// nearest-enemy selector ping-pongs every time the numbers cross.
			const TArray<FAIBTargetScoreInput> C = { Seen(1000.f), Seen(950.f) };
			TestEqual(TEXT("holds"), FAIBTargetPolicy::Choose(C, 0, Window), 0);
		});

		It("switches when the challenger is clearly better", [this]()
		{
			// "is there any other immediate threat facing me right now" — the incumbent
			// is a distant memory, the challenger is visible and on top of us.
			const TArray<FAIBTargetScoreInput> C = { Remembered(3800.f, 10.f), Seen(200.f) };
			TestEqual(TEXT("switches"), FAIBTargetPolicy::Choose(C, 0, Window), 1);
		});

		It("lets a runner go — the incumbent decays as it leaves and goes unseen", [this]()
		{
			// "but if he managed to run away". Same two candidates, the incumbent simply
			// gets further away and staler until it no longer clears the margin.
			const TArray<FAIBTargetScoreInput> Close = { Seen(600.f), Seen(900.f) };
			TestEqual(TEXT("still ours while he is near"),
				FAIBTargetPolicy::Choose(Close, 0, Window), 0);

			const TArray<FAIBTargetScoreInput> Fled = { Remembered(3900.f, 12.f), Seen(900.f) };
			TestEqual(TEXT("gone: take the one still here"),
				FAIBTargetPolicy::Choose(Fled, 0, Window), 1);
		});

		It("does not defend an incumbent that is no longer a candidate", [this]()
		{
			// Dead or aged out: the caller has already dropped it, so an out-of-range
			// index must not keep the slot or crash.
			const TArray<FAIBTargetScoreInput> C = { Seen(1500.f) };
			TestEqual(TEXT("takes the survivor"), FAIBTargetPolicy::Choose(C, 7, Window), 0);
		});

		It("answers nothing when it believes in nobody", [this]()
		{
			const TArray<FAIBTargetScoreInput> None;
			TestEqual(TEXT("no target"), FAIBTargetPolicy::Choose(None, INDEX_NONE, Window), INDEX_NONE);
			TestEqual(TEXT("not even a stale incumbent"),
				FAIBTargetPolicy::Choose(None, 0, Window), INDEX_NONE);
		});
	});

	Describe("the memory window is the tier's", [this]()
	{
		It("decays a stale lead faster for a short memory than a long one", [this]()
		{
			// A Recruit forgets sooner than a Spartan, from ONE number, with no second
			// definition of freshness anywhere.
			const FAIBTargetScoreInput Old = Remembered(1000.f, 7.f);
			TestTrue(TEXT("short memory scores it lower"),
				FAIBTargetPolicy::Score(Old, 8.f) < FAIBTargetPolicy::Score(Old, 20.f));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
