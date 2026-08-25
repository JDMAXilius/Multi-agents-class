#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTypes.h"
#include "Perception/AIBReactionClock.h"
#include "Perception/AIBSensorium.h"
#include "Perception/AIBTargetMemory.h"

/**
 * The fair envelope, proven worldless — no UWorld, no actors, no perception component.
 * Time is a number we pass; "actors" are null weak pointers where identity is irrelevant.
 * Every assertion cites the FAIRPLAY law it pins. Latency draws are made deterministic by
 * configuring Min == Max — the draw itself is then the assertion's known quantity.
 */
BEGIN_DEFINE_SPEC(FAIBSensoriumSpec, "AIBot.Sim.Sensorium",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	FAIBStimulus Sight(const FVector& Where) const
	{
		FAIBStimulus S;
		S.Kind = EAIBStimulusKind::SightGained;
		S.Location = Where;
		return S;
	}

END_DEFINE_SPEC(FAIBSensoriumSpec)

void FAIBSensoriumSpec::Define()
{
	Describe("the reaction clock (F1/F2)", [this]()
	{
		It("clamps a sub-floor draw UP to the 200ms floor — the one clamp site", [this]()
		{
			FAIBReactionClock Clock;
			Clock.Push(Sight(FVector::ZeroVector), /*Now=*/10.0, /*Drawn=*/0.05f);

			TArray<FAIBStimulus> Matured;
			// One frame before the floor: the law says the brain must not know yet.
			Clock.PopMatured(10.0 + AIB::MinReactionSeconds - 0.001, Matured);
			TestEqual(TEXT("nothing before the floor"), Matured.Num(), 0);

			Clock.PopMatured(10.0 + AIB::MinReactionSeconds + 0.001, Matured);
			TestEqual(TEXT("matured just past the floor"), Matured.Num(), 1);
		});

		It("holds a stimulus exactly its drawn latency, no shortcut", [this]()
		{
			FAIBReactionClock Clock;
			Clock.Push(Sight(FVector::ZeroVector), 5.0, 0.45f);

			TArray<FAIBStimulus> Matured;
			Clock.PopMatured(5.44, Matured);
			TestEqual(TEXT("still pending at 0.44"), Matured.Num(), 0);
			TestEqual(TEXT("queue holds it"), Clock.NumPending(), 1);

			Clock.PopMatured(5.46, Matured);
			TestEqual(TEXT("matured at 0.46"), Matured.Num(), 1);
			TestEqual(TEXT("queue drained"), Clock.NumPending(), 0);
		});

		It("pops in maturity order even when pushed out of order", [this]()
		{
			FAIBReactionClock Clock;
			FAIBStimulus Slow = Sight(FVector(1, 0, 0));
			FAIBStimulus Fast = Sight(FVector(2, 0, 0));
			Clock.Push(Slow, 0.0, 1.0f);   // matures at 1.0
			Clock.Push(Fast, 0.0, 0.3f);   // pushed later, matures FIRST at 0.3

			TArray<FAIBStimulus> Matured;
			Clock.PopMatured(2.0, Matured);
			TestEqual(TEXT("both matured"), Matured.Num(), 2);
			TestEqual(TEXT("the fast one leads"), Matured[0].Location.X, 2.0);
			TestEqual(TEXT("the slow one follows"), Matured[1].Location.X, 1.0);
		});

		It("records the fairness anchor: event time and maturity both ride the stimulus", [this]()
		{
			FAIBReactionClock Clock;
			Clock.Push(Sight(FVector::ZeroVector), 7.0, 0.30f);

			TArray<FAIBStimulus> Matured;
			Clock.PopMatured(8.0, Matured);
			TestEqual(TEXT("one out"), Matured.Num(), 1);
			TestEqual(TEXT("event stamped at push"), Matured[0].EventSeconds, 7.0);
			TestEqual(TEXT("maturity = event + latency"), Matured[0].MatureAtSeconds, 7.3);
		});
	});

	Describe("target memory (F5)", [this]()
	{
		It("has no memory until told, and reports a negative age", [this]()
		{
			FAIBTargetMemory Memory;
			FVector Where;
			TestFalse(TEXT("empty is never fresh"), Memory.GetFresh(100.0, 16.f, Where));
			TestTrue(TEXT("age is negative"), Memory.AgeSeconds(100.0) < 0.f);
		});

		It("goes stale at the window edge — infinite memory is banned", [this]()
		{
			// A null actor cannot satisfy GetFresh (a despawned enemy is not worth
			// searching for), so the position-freshness rule is pinned through age.
			FAIBTargetMemory Memory;
			Memory.Remember(nullptr, FVector(100, 0, 0), 50.0);
			TestEqual(TEXT("age at +10"), Memory.AgeSeconds(60.0), 10.f);
			TestEqual(TEXT("age at +20"), Memory.AgeSeconds(70.0), 20.f);

			FVector Where;
			TestFalse(TEXT("dead/despawned source is never fresh"), Memory.GetFresh(51.0, 16.f, Where));

			Memory.Forget();
			TestTrue(TEXT("forget clears the age"), Memory.AgeSeconds(70.0) < 0.f);
		});
	});

	Describe("the sensorium (F2 end to end)", [this]()
	{
		It("does not let the brain see a sighting before it matures", [this]()
		{
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.30f, 0.30f); // deterministic draw
			Sensorium.NoteSighting(nullptr, FVector(5, 5, 0), 1.0);

			Sensorium.Pump(1.25);
			TestFalse(TEXT("blind at 0.25"), Sensorium.HasVisibleTarget());
			TestEqual(TEXT("still queued"), Sensorium.NumPendingStimuli(), 1);

			Sensorium.Pump(1.35);
			// Null source stays null, but the queue drained and memory was written —
			// the mechanics matured on schedule.
			TestEqual(TEXT("queue drained at 0.35"), Sensorium.NumPendingStimuli(), 0);
			TestEqual(TEXT("latency recorded for the fairness line"),
				Sensorium.LastMaturedLatencySeconds(), 0.30f);
		});

		It("keeps a grenade invisible until the SAME clock matures it — no side channel", [this]()
		{
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);
			// Thrown at t=2.0, detonating at t=3.2.
			Sensorium.NoteIncomingBlast(FVector(10, 0, 0), 300.f, 3.2, 2.0);

			FVector Center;
			float Radius = 0.f;
			Sensorium.Pump(2.2);
			TestFalse(TEXT("unknown before maturity — the wall-dodge ban"),
				Sensorium.GetIncomingBlast(2.2, Center, Radius));

			Sensorium.Pump(2.3);
			TestTrue(TEXT("known after maturity"), Sensorium.GetIncomingBlast(2.3, Center, Radius));
			TestEqual(TEXT("where"), Center.X, 10.0);
			TestEqual(TEXT("how big"), Radius, 300.f);
		});

		It("stops reporting a blast the moment it has detonated", [this]()
		{
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);
			Sensorium.NoteIncomingBlast(FVector(10, 0, 0), 300.f, 3.2, 2.0);
			Sensorium.Pump(2.3);

			FVector Center;
			float Radius = 0.f;
			TestTrue(TEXT("live before boom"), Sensorium.GetIncomingBlast(3.1, Center, Radius));
			TestFalse(TEXT("gone at boom — dodging afterwards reads as broken"),
				Sensorium.GetIncomingBlast(3.2, Center, Radius));
		});

		It("keeps the second grenade's own detonation time — no overwrite", [this]()
		{
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);
			Sensorium.NoteIncomingBlast(FVector(10, 0, 0), 300.f, /*boom=*/3.0, /*now=*/2.0);
			Sensorium.NoteIncomingBlast(FVector(90, 0, 0), 300.f, /*boom=*/9.0, /*now=*/2.1);

			Sensorium.Pump(2.4); // both matured
			FVector Center;
			float Radius = 0.f;
			// After the FIRST detonates, the SECOND must still threaten with ITS location.
			TestTrue(TEXT("second blast alive after first boom"),
				Sensorium.GetIncomingBlast(3.5, Center, Radius));
			TestEqual(TEXT("and it is the second one"), Center.X, 90.0);
		});

		It("resets to blind and empty", [this]()
		{
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);
			Sensorium.NoteSighting(nullptr, FVector(5, 5, 0), 1.0);
			Sensorium.NoteIncomingBlast(FVector(10, 0, 0), 300.f, 9.0, 1.0);
			Sensorium.Reset();

			FVector Center;
			float Radius = 0.f;
			TestEqual(TEXT("no stimuli"), Sensorium.NumPendingStimuli(), 0);
			TestFalse(TEXT("no target"), Sensorium.HasVisibleTarget());
			TestFalse(TEXT("no blast"), Sensorium.GetIncomingBlast(2.0, Center, Radius));
			TestTrue(TEXT("no memory"), Sensorium.MemoryAgeSeconds(2.0) < 0.f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
