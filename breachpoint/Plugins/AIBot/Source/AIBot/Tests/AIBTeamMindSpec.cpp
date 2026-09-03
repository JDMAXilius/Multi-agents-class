#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Brain/AIBAmbitionEngine.h"
#include "Data/AIBDataRows.h"
#include "GameFramework/Actor.h"
#include "Team/AIBSightingLedger.h"
#include "Team/AIBVisitHeat.h"

/**
 * PHASE 12 (AIB23) — the Team Mind's two other members, proven headless.
 * VISIT HEAT: a stamp reads 1 in its cell and 0 elsewhere (1); it decays by the row's
 * constant (2); it is TEAM-ONLY — an enemy's footsteps read 0, self always reads (3);
 * prune drops the cold (4). SIGHTING LEDGER: a report is never relayed to its own
 * reporter or across alliances (5); a stale entry (reporter lost sight) is never relayed
 * (6); the freshest stamp per target wins and the ORIGINAL stamp travels untouched (7).
 * The row defaults the csv must mirror (8).
 */
BEGIN_DEFINE_SPEC(FAIBTeamMindSpec, "AIBot.Sim.TeamMind",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	UObject* BotA = nullptr;
	UObject* BotB = nullptr;
	AActor* Enemy = nullptr;

	static bool Allies(const AActor*, const AActor*) { return true; }
	static bool Enemies(const AActor*, const AActor*) { return false; }
	static constexpr float Cell = 500.f;
	static constexpr float Decay = 30.f;

END_DEFINE_SPEC(FAIBTeamMindSpec)

void FAIBTeamMindSpec::Define()
{
	BeforeEach([this]()
	{
		BotA = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient); BotA->AddToRoot();
		BotB = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient); BotB->AddToRoot();
		Enemy = NewObject<AActor>(GetTransientPackage(), NAME_None, RF_Transient); Enemy->AddToRoot();
	});

	AfterEach([this]()
	{
		if (BotA) { BotA->RemoveFromRoot(); BotA = nullptr; }
		if (BotB) { BotB->RemoveFromRoot(); BotB = nullptr; }
		if (Enemy) { Enemy->RemoveFromRoot(); Enemy = nullptr; }
	});

	Describe("the visit heat grid", [this]()
	{
		It("reads hot in the stamped cell and cold everywhere else", [this]()
		{
			FAIBVisitHeat Heat;
			Heat.Stamp(FObjectKey(BotA), nullptr, FVector(100.f, 100.f, 0.f), 10.0, Cell);
			TestEqual(TEXT("same cell, same instant"), Heat.HeatAt(FObjectKey(BotB), nullptr, FVector(400.f, 400.f, 0.f), 10.0, Cell, Decay, &Allies), 1.f, 0.001f);
			TestEqual(TEXT("next cell"), Heat.HeatAt(FObjectKey(BotB), nullptr, FVector(600.f, 100.f, 0.f), 10.0, Cell, Decay, &Allies), 0.f);
			TestEqual(TEXT("a storey up"), Heat.HeatAt(FObjectKey(BotB), nullptr, FVector(100.f, 100.f, 900.f), 10.0, Cell, Decay, &Allies), 0.f);
		});

		It("decays by the row's constant — 1/e after one, ~0 after many", [this]()
		{
			FAIBVisitHeat Heat;
			Heat.Stamp(FObjectKey(BotA), nullptr, FVector::ZeroVector, 0.0, Cell);
			TestEqual(TEXT("one decay"), Heat.HeatAt(FObjectKey(BotA), nullptr, FVector::ZeroVector, Decay, Cell, Decay, &Allies), 0.3679f, 0.001f);
			TestTrue(TEXT("cold later"), Heat.HeatAt(FObjectKey(BotA), nullptr, FVector::ZeroVector, 10.0 * Decay, Cell, Decay, &Allies) < 0.001f);
		});

		It("is team-only — an enemy's footsteps read 0, self always reads", [this]()
		{
			FAIBVisitHeat Heat;
			Heat.Stamp(FObjectKey(BotA), nullptr, FVector::ZeroVector, 0.0, Cell);
			TestEqual(TEXT("a stranger reads nothing"), Heat.HeatAt(FObjectKey(BotB), nullptr, FVector::ZeroVector, 0.0, Cell, Decay, &Enemies), 0.f);
			TestEqual(TEXT("the stamper reads its own under any predicate"), Heat.HeatAt(FObjectKey(BotA), nullptr, FVector::ZeroVector, 0.0, Cell, Decay, &Enemies), 1.f, 0.001f);
		});

		It("prunes the cold and keeps the grid small", [this]()
		{
			FAIBVisitHeat Heat;
			Heat.Stamp(FObjectKey(BotA), nullptr, FVector::ZeroVector, 0.0, Cell);
			Heat.Stamp(FObjectKey(BotA), nullptr, FVector(5000.f, 0.f, 0.f), 100.0, Cell);
			Heat.Prune(200.0, Decay);
			TestEqual(TEXT("one cell left"), Heat.Cells.Num(), 1);
		});
	});

	Describe("the sighting ledger", [this]()
	{
		It("never relays a report to its own reporter or across alliances", [this]()
		{
			FAIBSightingLedger Ledger;
			Ledger.Publish(FObjectKey(BotA), nullptr, TEXT("A"), Enemy, FVector(1.f, 2.f, 3.f), 9.0, 10.0);
			int32 Seen = 0;
			Ledger.ForEachReport(FObjectKey(BotA), nullptr, 10.0, 0.5f, &Allies, [&](const FAIBSighting&) { ++Seen; });
			TestEqual(TEXT("not to self"), Seen, 0);
			Ledger.ForEachReport(FObjectKey(BotB), nullptr, 10.0, 0.5f, &Enemies, [&](const FAIBSighting&) { ++Seen; });
			TestEqual(TEXT("not to a stranger"), Seen, 0);
			Ledger.ForEachReport(FObjectKey(BotB), nullptr, 10.0, 0.5f, &Allies, [&](const FAIBSighting&) { ++Seen; });
			TestEqual(TEXT("to a teammate"), Seen, 1);
		});

		It("never relays a stale entry — a reporter that lost sight stops reporting", [this]()
		{
			FAIBSightingLedger Ledger;
			Ledger.Publish(FObjectKey(BotA), nullptr, TEXT("A"), Enemy, FVector::ZeroVector, 9.0, 10.0);
			int32 Seen = 0;
			Ledger.ForEachReport(FObjectKey(BotB), nullptr, 10.4, 0.5f, &Allies, [&](const FAIBSighting&) { ++Seen; });
			TestEqual(TEXT("fresh"), Seen, 1);
			Ledger.ForEachReport(FObjectKey(BotB), nullptr, 10.6, 0.5f, &Allies, [&](const FAIBSighting&) { ++Seen; });
			TestEqual(TEXT("stale: not relayed"), Seen, 1);
			Ledger.Prune(10.6, 0.5f);
			TestEqual(TEXT("and pruned"), Ledger.Reports.Num(), 0);
		});

		It("relays the freshest stamp per target, the ORIGINAL stamp untouched", [this]()
		{
			FAIBSightingLedger Ledger;
			UObject* BotC = NewObject<UAIBAmbitionEngine>(GetTransientPackage(), NAME_None, RF_Transient);
			BotC->AddToRoot();
			Ledger.Publish(FObjectKey(BotA), nullptr, TEXT("A"), Enemy, FVector(1.f, 0.f, 0.f), 7.0, 10.0);
			Ledger.Publish(FObjectKey(BotC), nullptr, TEXT("C"), Enemy, FVector(2.f, 0.f, 0.f), 9.0, 10.0);
			int32 Seen = 0;
			double Stamp = -1.0;
			FString From;
			Ledger.ForEachReport(FObjectKey(BotB), nullptr, 10.0, 0.5f, &Allies, [&](const FAIBSighting& R)
			{
				++Seen; Stamp = R.SeenAtSeconds; From = R.ReporterName;
			});
			TestEqual(TEXT("one per target"), Seen, 1);
			TestEqual(TEXT("the fresher seeing"), Stamp, 9.0);
			TestEqual(TEXT("from C"), From, FString(TEXT("C")));
			BotC->RemoveFromRoot();
		});
	});

	It("carries the row defaults the csv must mirror", [this]()
	{
		const FAIBTierRow Row;
		TestEqual(TEXT("ClaimMinHoldSeconds"), Row.ClaimMinHoldSeconds, 2.f);
		TestEqual(TEXT("TeamReportIntervalSeconds"), Row.TeamReportIntervalSeconds, 1.f);
		TestEqual(TEXT("TeamReportStaleSeconds"), Row.TeamReportStaleSeconds, 0.5f);
		TestEqual(TEXT("VisitHeatCellUU"), Row.VisitHeatCellUU, 500.f);
		TestEqual(TEXT("VisitHeatDecaySeconds"), Row.VisitHeatDecaySeconds, 30.f);
		TestEqual(TEXT("VisitHeatDrawSamples"), Row.VisitHeatDrawSamples, 3);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
