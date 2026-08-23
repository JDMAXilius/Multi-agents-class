#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/BNBotBrain.h"
#include "Data/BNDataRows.h"

/**
 * THE BRAIN, which is the one part of the bot stack that can be tested at all without a world:
 * UBNBotBrain is headless by contract — no GetWorld, no clock, no actors, only facts, rows and a
 * time its caller hands in. That contract is what makes this file possible, and this file is what
 * keeps the contract honest.
 *
 * It exists because of a real bug: the Survive interrupt was written as a utility RATIO, and the
 * critic proved the form unreachable with the shipped weights (Survive's ceiling of 1.2 can never
 * double Fight's 1.0), which left a bot at 5% health standing and firing through its whole commit
 * window. Nothing caught that until a person read the arithmetic. The last spec below is that bug,
 * in the form it would have failed in.
 *
 * The rows come from UBNBotBrain::DefaultRow, NOT from literals here: DT_BNBotAmbitions mirrors
 * those same defaults, so a spec with its own copy of the numbers would pass while the shipped
 * decision changed underneath it.
 */
BEGIN_DEFINE_SPEC(FBNBotBrainSpec, "BreachpointNext.Sim.BotBrain",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	FBNBotAmbitionRow Fight;
	FBNBotAmbitionRow Survive;
	FBNBotAmbitionRow Roam;

	UBNBotBrain* Brain = nullptr;

	/** One call, with the shipped rows. Returns whether the ambition CHANGED. */
	bool Rescore(const FBNBotFacts& Facts, double NowSeconds)
	{
		return Brain->Rescore(Facts, Fight, Survive, Roam, NowSeconds);
	}

	/** int32, not the enum: TestEqual's overload set is explicit about the types it prints, and a
	 *  spec is not the place to discover which template a given engine version resolved. */
	int32 Ambition() const { return static_cast<int32>(Brain->GetAmbition()); }

	static int32 Want(EBNBotAmbition Ambition) { return static_cast<int32>(Ambition); }

	static FBNBotFacts MakeFacts(bool bHasTarget, float HealthNorm)
	{
		FBNBotFacts Facts;
		Facts.bHasTarget = bHasTarget;
		Facts.HealthNorm = HealthNorm;
		Facts.DistToTargetNorm = 0.5f;
		return Facts;
	}

END_DEFINE_SPEC(FBNBotBrainSpec)

void FBNBotBrainSpec::Define()
{
	BeforeEach([this]()
	{
		Fight = UBNBotBrain::DefaultRow(EBNBotAmbition::Fight);
		Survive = UBNBotBrain::DefaultRow(EBNBotAmbition::Survive);
		Roam = UBNBotBrain::DefaultRow(EBNBotAmbition::Roam);

		Brain = NewObject<UBNBotBrain>(GetTransientPackage(), NAME_None, RF_Transient);
		Brain->AddToRoot();
	});

	AfterEach([this]()
	{
		if (Brain)
		{
			Brain->RemoveFromRoot();
			Brain = nullptr;
		}
	});

	It("wants nothing in particular with no target and full health", [this]()
	{
		Rescore(MakeFacts(/*bHasTarget=*/false, 1.f), 0.0);
		TestEqual(TEXT("ambition"), Ambition(), Want(EBNBotAmbition::Roam));
	});

	It("wants to fight the moment a target is in sight", [this]()
	{
		const bool bChanged = Rescore(MakeFacts(true, 1.f), 0.0);
		TestTrue(TEXT("the first selection is a change"), bChanged);
		TestEqual(TEXT("ambition"), Ambition(), Want(EBNBotAmbition::Fight));
	});

	It("holds an ambition for its commit window even after the reason expires", [this]()
	{
		Rescore(MakeFacts(true, 1.f), 0.0);

		// The target is gone one second in. Fight's window is 3s, so the bot must still be
		// committed — this is the hysteresis that makes bots look decisive instead of twitchy.
		const bool bChanged = Rescore(MakeFacts(false, 1.f), 1.0);
		TestFalse(TEXT("no change inside the commit window"), bChanged);
		TestEqual(TEXT("ambition"), Ambition(), Want(EBNBotAmbition::Fight));
	});

	It("re-selects once the commit window has passed", [this]()
	{
		Rescore(MakeFacts(true, 1.f), 0.0);
		const bool bChanged = Rescore(MakeFacts(false, 1.f), 10.0);
		TestTrue(TEXT("the window is over, so the ambition may move"), bChanged);
		TestEqual(TEXT("ambition"), Ambition(), Want(EBNBotAmbition::Roam));
	});

	It("reports no change when the winner is what it already wanted", [this]()
	{
		Rescore(MakeFacts(true, 1.f), 0.0);
		const bool bChanged = Rescore(MakeFacts(true, 1.f), 10.0);
		TestFalse(TEXT("same winner after the window is not news"), bChanged);
		TestEqual(TEXT("ambition"), Ambition(), Want(EBNBotAmbition::Fight));
	});

	It("decays a hurt-but-safe bot to roaming rather than cowering", [this]()
	{
		// Survive's shape carries a -0.5 term when nothing is in sight, so a bot that is hurt and
		// alone sinks below Roam's floor and goes back to wandering. Half health, no threat.
		Rescore(MakeFacts(false, 0.5f), 0.0);
		TestEqual(TEXT("ambition"), Ambition(), Want(EBNBotAmbition::Roam));
	});

	It("BREAKS a commit to survive when health falls under the row's threshold", [this]()
	{
		// THE REGRESSION. This began life as a utility ratio the shipped weights could never
		// satisfy: a bot at 5% health kept firing through its whole 3-second Fight commit because
		// Survive's 1.2 ceiling can never double Fight's 1.0. The fix was one number on the row,
		// and this is the case that number exists for.
		Rescore(MakeFacts(true, 1.f), 0.0);
		TestEqual(TEXT("fighting first"), Ambition(), Want(EBNBotAmbition::Fight));

		const bool bChanged = Rescore(MakeFacts(true, 0.05f), 1.0);
		TestTrue(TEXT("the interrupt is a change, mid-window"), bChanged);
		TestEqual(TEXT("ambition"), Ambition(), Want(EBNBotAmbition::Survive));
	});

	It("does not interrupt when health is above the threshold", [this]()
	{
		Rescore(MakeFacts(true, 1.f), 0.0);
		const bool bChanged = Rescore(MakeFacts(true, 0.9f), 1.0);
		TestFalse(TEXT("healthy enough to keep fighting"), bChanged);
		TestEqual(TEXT("ambition"), Ambition(), Want(EBNBotAmbition::Fight));
	});

	It("names the consideration that won, for the log line a human reads", [this]()
	{
		Rescore(MakeFacts(true, 1.f), 0.0);
		TestEqual(TEXT("consideration"), Brain->GetWinningConsideration(), FString(TEXT("a target is in sight")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
