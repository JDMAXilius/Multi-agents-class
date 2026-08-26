#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Components/SceneComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBTypes.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Perception/AIBReactionClock.h"
#include "Perception/AIBSensorium.h"
#include "Perception/AIBTargetMemory.h"

/**
 * The fair envelope, proven at two altitudes. Pure clock math runs worldless (time is a
 * parameter). Identity-bearing behaviour — visibility, memory freshness, the stale-loss
 * guard — runs against REAL spawned actors, because the first draft of this suite passed
 * null actors everywhere and its visibility assertions passed vacuously: "blind at
 * 0.25s" held with the reaction clock deleted (W-REVIEW F-5.1). The world fixture is the
 * host damage spec's proven shape. Latency draws are deterministic via Min == Max.
 */
BEGIN_DEFINE_SPEC(FAIBSensoriumSpec, "AIBot.Sim.Sensorium",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	UWorld* World = nullptr;

	bool BuildWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World)
		{
			AddError(TEXT("UWorld::CreateWorld returned null."));
			return false;
		}
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		return true;
	}

	AActor* SpawnBody(const FVector& Where = FVector::ZeroVector)
	{
		AActor* Actor = World ? World->SpawnActor<AActor>(Where, FRotator::ZeroRotator) : nullptr;
		if (!Actor)
		{
			AddError(TEXT("SpawnActor failed."));
			return nullptr;
		}

		// A BARE AActor HAS NO ROOT COMPONENT, AND WITHOUT ONE IT CANNOT HOLD A POSITION.
		// AActor::GetActorLocation() is TemplateGetActorLocation(RootComponent), which returns
		// FVector::ZeroVector when that pointer is null — so the spawn transform above is
		// discarded and every later SetActorLocation is a silent no-op. That is why this
		// fixture read 0.0 for both 'belief at the enemy's position' (expected 5) and 'belief
		// tracks at pump cadence' (expected 50): not a sensorium bug, a body with nowhere to be.
		//
		// Giving it a real scene root is the whole fix. Same idiom the host module's damage spec
		// already uses for its runtime-constructed actors.
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(Where);

		// Prove it took. A fixture that silently cannot move is indistinguishable from a
		// behaviour change, and that ambiguity already cost two sessions.
		if (!Actor->GetActorLocation().Equals(Where, 0.01f))
		{
			AddError(FString::Printf(TEXT("SpawnBody could not place the actor at %s (it reports %s)."),
				*Where.ToString(), *Actor->GetActorLocation().ToString()));
		}
		return Actor;
	}

	void DestroyWorld()
	{
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World = nullptr;
		}
	}

	FAIBStimulus Sight(AActor* Who, const FVector& Where) const
	{
		FAIBStimulus S;
		S.Kind = EAIBStimulusKind::SightGained;
		S.Source = Who;
		S.Location = Where;
		return S;
	}

END_DEFINE_SPEC(FAIBSensoriumSpec)

void FAIBSensoriumSpec::Define()
{
	BeforeEach([this]() { BuildWorld(); });
	AfterEach([this]() { DestroyWorld(); });

	Describe("the reaction clock (F1/F2)", [this]()
	{
		It("clamps a sub-floor draw UP to the 200ms floor — the one clamp site", [this]()
		{
			FAIBReactionClock Clock;
			Clock.Push(Sight(nullptr, FVector::ZeroVector), 10.0, 0.05f);

			TArray<FAIBStimulus> Matured;
			Clock.PopMatured(10.0 + AIB::MinReactionSeconds - 0.001, Matured);
			TestEqual(TEXT("nothing before the floor"), Matured.Num(), 0);

			Clock.PopMatured(10.0 + AIB::MinReactionSeconds + 0.001, Matured);
			TestEqual(TEXT("matured just past the floor"), Matured.Num(), 1);
		});

		It("holds a stimulus exactly its drawn latency, no shortcut", [this]()
		{
			FAIBReactionClock Clock;
			Clock.Push(Sight(nullptr, FVector::ZeroVector), 5.0, 0.45f);

			TArray<FAIBStimulus> Matured;
			Clock.PopMatured(5.44, Matured);
			TestEqual(TEXT("still pending at 0.44"), Matured.Num(), 0);
			Clock.PopMatured(5.46, Matured);
			TestEqual(TEXT("matured at 0.46"), Matured.Num(), 1);
		});

		It("pops in maturity order even when pushed out of order", [this]()
		{
			FAIBReactionClock Clock;
			Clock.Push(Sight(nullptr, FVector(1, 0, 0)), 0.0, 1.0f);
			Clock.Push(Sight(nullptr, FVector(2, 0, 0)), 0.0, 0.3f);

			TArray<FAIBStimulus> Matured;
			Clock.PopMatured(2.0, Matured);
			TestEqual(TEXT("both matured"), Matured.Num(), 2);
			TestEqual(TEXT("the fast one leads"), Matured[0].Location.X, 2.0);
		});

		It("keeps equal maturities FIFO — the tie rule the sort must not break", [this]()
		{
			FAIBReactionClock Clock;
			Clock.Push(Sight(nullptr, FVector(1, 0, 0)), 0.0, 0.3f);
			Clock.Push(Sight(nullptr, FVector(2, 0, 0)), 0.0, 0.3f);

			TArray<FAIBStimulus> Matured;
			Clock.PopMatured(1.0, Matured);
			TestEqual(TEXT("both out"), Matured.Num(), 2);
			TestEqual(TEXT("first pushed, first out"), Matured[0].Location.X, 1.0);
		});

		It("stamps push time as the event, and honours a caller's earlier truth", [this]()
		{
			FAIBReactionClock Clock;
			Clock.Push(Sight(nullptr, FVector::ZeroVector), 7.0, 0.30f);

			FAIBStimulus Backdated = Sight(nullptr, FVector::ZeroVector);
			Backdated.EventSeconds = 6.5; // happened half a second before it was noted
			Clock.Push(Backdated, 7.0, 0.30f);

			TArray<FAIBStimulus> Matured;
			Clock.PopMatured(8.0, Matured);
			TestEqual(TEXT("two out"), Matured.Num(), 2);
			TestEqual(TEXT("unset event stamped at push"), Matured[0].EventSeconds, 7.0);
			TestEqual(TEXT("backdated event preserved"), Matured[1].EventSeconds, 6.5);
			// Both matured from PUSH time: backdating labels, it never accelerates.
			TestEqual(TEXT("maturity from push, not the backdate"),
				Matured[1].MatureAtSeconds, Matured[0].MatureAtSeconds);
		});

		It("drops the oldest at the cap instead of growing without bound", [this]()
		{
			FAIBReactionClock Clock;
			for (int32 i = 0; i < AIB::MaxPendingStimuli + 10; ++i)
			{
				Clock.Push(Sight(nullptr, FVector(i, 0, 0)), static_cast<double>(i) * 0.001, 5.f);
			}
			TestEqual(TEXT("capped"), Clock.NumPending(), AIB::MaxPendingStimuli);

			TArray<FAIBStimulus> Matured;
			Clock.PopMatured(100.0, Matured);
			TestEqual(TEXT("the survivors are the NEWEST"),
				Matured[0].Location.X, 10.0); // 0..9 were dropped
		});
	});

	Describe("target memory (F5)", [this]()
	{
		It("remembers a real actor and reports it fresh inside the window", [this]()
		{
			AActor* Enemy = SpawnBody();
			FAIBTargetMemory Memory;
			Memory.Remember(Enemy, FVector(100, 0, 0), 50.0);

			FVector Where;
			TestTrue(TEXT("fresh at +10 of a 16s window"), Memory.GetFresh(60.0, 16.f, Where));
			TestEqual(TEXT("the remembered spot"), Where.X, 100.0);
			TestEqual(TEXT("age"), Memory.AgeSeconds(60.0), 10.f);
		});

		It("goes stale at the window edge exactly — >= is the law's edge", [this]()
		{
			AActor* Enemy = SpawnBody();
			FAIBTargetMemory Memory;
			Memory.Remember(Enemy, FVector(100, 0, 0), 50.0);

			FVector Where;
			TestTrue(TEXT("fresh just inside"), Memory.GetFresh(65.99, 16.f, Where));
			TestFalse(TEXT("stale AT the edge"), Memory.GetFresh(66.0, 16.f, Where));
		});

		It("clamps any caller window to the module ceiling — FLT_MAX buys nothing", [this]()
		{
			AActor* Enemy = SpawnBody();
			FAIBTargetMemory Memory;
			Memory.Remember(Enemy, FVector(100, 0, 0), 50.0);

			FVector Where;
			TestTrue(TEXT("fresh inside the ceiling"),
				Memory.GetFresh(50.0 + AIB::MaxMemorySeconds - 0.5, FLT_MAX, Where));
			TestFalse(TEXT("infinite window still dies at the ceiling"),
				Memory.GetFresh(50.0 + AIB::MaxMemorySeconds + 0.5, FLT_MAX, Where));
		});

		It("refuses a null source — no memory of nobody", [this]()
		{
			FAIBTargetMemory Memory;
			Memory.Remember(nullptr, FVector(100, 0, 0), 50.0);
			TestTrue(TEXT("age says empty"), Memory.AgeSeconds(51.0) < 0.f);
		});

		It("reads as NO memory the moment the remembered actor is destroyed", [this]()
		{
			AActor* Enemy = SpawnBody();
			FAIBTargetMemory Memory;
			Memory.Remember(Enemy, FVector(100, 0, 0), 50.0);

			Enemy->Destroy();
			// Both readers agree — the age is not allowed to say "fresh" while GetFresh
			// says "nothing" (the split state W-REVIEW F-2.1 constructed).
			FVector Where;
			TestFalse(TEXT("not fresh"), Memory.GetFresh(51.0, 16.f, Where));
			TestTrue(TEXT("age agrees: no memory"), Memory.AgeSeconds(51.0) < 0.f);
		});
	});

	Describe("the sensorium (F2 end to end, real actors)", [this]()
	{
		It("matures a sighting into a visible target — and not before the clock says", [this]()
		{
			AActor* Enemy = SpawnBody(FVector(5, 5, 0));
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.30f, 0.30f);
			Sensorium.NoteSighting(Enemy, FVector(5, 5, 0), 1.0);

			Sensorium.Pump(1.25);
			TestFalse(TEXT("blind at 0.25"), Sensorium.HasVisibleTarget());

			Sensorium.Pump(1.35);
			TestTrue(TEXT("seeing at 0.35"), Sensorium.HasVisibleTarget());
			TestEqual(TEXT("and it is the enemy"), Sensorium.GetVisibleTarget(), Enemy);
			TestTrue(TEXT("sight is current"), Sensorium.IsSightCurrent());
			TestEqual(TEXT("belief at the enemy's position"),
				Sensorium.GetLastSeenLocation().X, 5.0);

			// THE BELIEF RULE, asserted head-on (the rung-2 failure was this re-sample
			// working against a fixture whose actor sat at the origin while the stimulus
			// claimed 5): while sight is CURRENT the belief follows the live actor at
			// pump cadence; the moment a loss is NOTED it freezes at the last spot.
			Enemy->SetActorLocation(FVector(50, 5, 0));
			Sensorium.Pump(1.45);
			TestEqual(TEXT("belief tracks at pump cadence while current"),
				Sensorium.GetLastSeenLocation().X, 50.0);

			Sensorium.NoteSightingLost(Enemy, FVector(60, 5, 0), 1.5);
			Enemy->SetActorLocation(FVector(900, 5, 0));
			Sensorium.Pump(1.55);
			TestEqual(TEXT("frozen at the loss spot, not the true position"),
				Sensorium.GetLastSeenLocation().X, 60.0);
		});

		It("ignores a stale loss that matured after a newer re-acquire (the corner peek)", [this]()
		{
			AActor* Enemy = SpawnBody();
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);

			// Acquire, then the peek: loss noted at 2.00 (slow draw would matter here,
			// but determinism gives both 0.25 — so re-order by NOTE time instead).
			Sensorium.NoteSighting(Enemy, FVector(5, 0, 0), 1.0);
			Sensorium.Pump(1.30);
			TestTrue(TEXT("acquired"), Sensorium.HasVisibleTarget());

			// Loss happens at 2.00; enemy steps back out and is re-sighted at 2.10.
			// With equal draws the gain matures at 2.35, the loss at 2.25 — benign
			// order. Force the malign order: backdate is not possible through Note*,
			// so emulate the engine's actual sequence — loss event, gain event, then
			// one pump after both matured. The gain's LATER event time must win.
			Sensorium.NoteSightingLost(Enemy, FVector(6, 0, 0), 2.00);
			Sensorium.NoteSighting(Enemy, FVector(7, 0, 0), 2.10);
			Sensorium.Pump(2.40);

			TestTrue(TEXT("still seeing — the older loss did not blind us"),
				Sensorium.HasVisibleTarget());
			TestTrue(TEXT("sight current again"), Sensorium.IsSightCurrent());
		});

		It("freezes tracking the moment a loss is NOTED — the juke honesty rule", [this]()
		{
			AActor* Enemy = SpawnBody();
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);
			Sensorium.NoteSighting(Enemy, FVector(5, 0, 0), 1.0);
			Sensorium.Pump(1.30);

			Sensorium.NoteSightingLost(Enemy, FVector(9, 0, 0), 2.0);
			// Loss not yet matured: the bot still believes — but belief is a SPOT now,
			// not a live track through the pillar.
			TestTrue(TEXT("still believes (visible)"), Sensorium.HasVisibleTarget());
			TestFalse(TEXT("but sight is no longer current"), Sensorium.IsSightCurrent());
			TestEqual(TEXT("held at the last seen spot"), Sensorium.GetLastSeenLocation().X, 9.0);

			Sensorium.Pump(2.30);
			TestFalse(TEXT("matured loss blanks visibility"), Sensorium.HasVisibleTarget());
			TestTrue(TEXT("and becomes memory"), Sensorium.MemoryAgeSeconds(2.35) >= 0.f);
		});

		It("supersedes a gain whose loss drew the faster reaction — the 100ms peek", [this]()
		{
			// THE ORDERING HOLE (W-REVIEW P3, fairness HIGH): peek at 1.00, re-hide at
			// 1.10. The gain draws 0.45s (matures 1.45); the loss draws 0.25s (matures
			// 1.35) — the loss matures FIRST, into an empty VisibleTarget, and the old
			// code dropped it on the floor. The surviving gain then live-tracked the
			// hidden enemy for SightMaxAge. Configure between notes makes the draw
			// inversion deterministic; the engine produces it on ~1 in 6 short peeks.
			AActor* Enemy = SpawnBody(FVector(5, 0, 0));
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.45f, 0.45f);
			Sensorium.NoteSighting(Enemy, FVector(5, 0, 0), 1.00);
			Sensorium.Configure(0.25f, 0.25f);
			Sensorium.NoteSightingLost(Enemy, FVector(6, 0, 0), 1.10);

			Sensorium.Pump(1.40); // loss matured; gain still pending
			TestFalse(TEXT("nothing visible before the gain matures"), Sensorium.HasVisibleTarget());

			Sensorium.Pump(1.50); // gain matured — and must arrive already superseded
			TestFalse(TEXT("the superseded gain must not become current sight"),
				Sensorium.HasVisibleTarget());
			TestTrue(TEXT("no acquisition was recorded for it"),
				Sensorium.LastAcquisitionLatencySeconds() < 0.f);

			// What the peek honestly earned: a memory at the gained spot, not a track.
			FVector Remembered;
			TestTrue(TEXT("the peek lands as fresh memory"),
				Sensorium.Memory().GetFresh(1.55, 16.f, Remembered));
			TestEqual(TEXT("memory at the seen spot"), Remembered.X, 5.0);

			// And the enemy moving behind the wall leaks nothing.
			Enemy->SetActorLocation(FVector(50, 0, 0));
			Sensorium.Pump(1.60);
			TestFalse(TEXT("still blind after the enemy moves unseen"), Sensorium.HasVisibleTarget());
		});

		It("lets ears place but not see, and never evict the watched enemy's memory", [this]()
		{
			AActor* Enemy = SpawnBody();
			AActor* Noisemaker = SpawnBody();
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);

			Sensorium.NoteSighting(Enemy, FVector(5, 0, 0), 1.0);
			Sensorium.Pump(1.30);

			Sensorium.NoteSound(Noisemaker, FVector(500, 0, 0), 2.0);
			Sensorium.Pump(2.30);

			TestEqual(TEXT("still watching the enemy"), Sensorium.GetVisibleTarget(), Enemy);
			TestTrue(TEXT("memory still the ENEMY's, not the noise"),
				Sensorium.Memory().Remembers(Enemy));

			// No focus -> a sound IS the new lead.
			Sensorium.Reset();
			Sensorium.Configure(0.25f, 0.25f);
			Sensorium.NoteSound(Noisemaker, FVector(500, 0, 0), 3.0);
			Sensorium.Pump(3.30);
			TestFalse(TEXT("hearing is not seeing"), Sensorium.HasVisibleTarget());
			TestTrue(TEXT("but it places"), Sensorium.Memory().Remembers(Noisemaker));
		});

		It("keeps a grenade invisible until the SAME clock matures it — no side channel", [this]()
		{
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);
			Sensorium.NoteIncomingBlast(FVector(10, 0, 0), 300.f, 3.2, 2.0);

			FVector Center;
			float Radius = 0.f;
			Sensorium.Pump(2.2);
			TestFalse(TEXT("unknown before maturity — the wall-dodge ban"),
				Sensorium.GetIncomingBlast(2.2, Center, Radius));

			Sensorium.Pump(2.3);
			TestTrue(TEXT("known after maturity"), Sensorium.GetIncomingBlast(2.3, Center, Radius));
			TestEqual(TEXT("where"), Center.X, 10.0);
		});

		It("keeps BOTH grenades: the imminent one wins, the survivor outlives its boom", [this]()
		{
			// THE ordering the first spec dodged (W-REVIEW B-2/F-5.3): near grenade A
			// detonates FIRST; far grenade B noted later. A single slot either dodged
			// toward B while A killed the bot, or forgot A once B detonated.
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);
			Sensorium.NoteIncomingBlast(FVector(10, 0, 0), 300.f, /*boom=*/3.0, /*now=*/2.0);
			Sensorium.NoteIncomingBlast(FVector(900, 0, 0), 300.f, /*boom=*/9.0, /*now=*/2.1);
			Sensorium.Pump(2.4);

			FVector Center;
			float Radius = 0.f;
			TestTrue(TEXT("a threat is known"), Sensorium.GetIncomingBlast(2.5, Center, Radius));
			TestEqual(TEXT("the IMMINENT one wins, not the last-matured"), Center.X, 10.0);

			// After A detonates, B must still threaten — the erasure bug.
			Sensorium.Pump(3.1);
			TestTrue(TEXT("the far grenade survives the near one's boom"),
				Sensorium.GetIncomingBlast(3.1, Center, Radius));
			TestEqual(TEXT("and it is the far one"), Center.X, 900.0);

			TestFalse(TEXT("all quiet after both"), Sensorium.GetIncomingBlast(9.5, Center, Radius));
		});

		It("measures the HONEST acquisition latency — pump delay in, no overwrite by noise", [this]()
		{
			AActor* Enemy = SpawnBody();
			AActor* Noisemaker = SpawnBody();
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);

			// Sight at 1.0 (matures 1.25) and a sound at 1.02 (matures 1.27), harvested
			// together by a LATE pump at 1.50. The instrument must report the sighting's
			// happened->surfaced gap (0.50), not the clamp (0.25), and not the sound's.
			Sensorium.NoteSighting(Enemy, FVector(5, 0, 0), 1.0);
			Sensorium.NoteSound(Noisemaker, FVector(500, 0, 0), 1.02);
			Sensorium.Pump(1.50);

			TestTrue(TEXT("acquired"), Sensorium.HasVisibleTarget());
			TestEqual(TEXT("latency = happened->surfaced, 0.50"),
				Sensorium.LastAcquisitionLatencySeconds(), 0.50f, 0.001f);
		});

		It("resets to blind and empty", [this]()
		{
			AActor* Enemy = SpawnBody();
			FAIBSensorium Sensorium;
			Sensorium.Configure(0.25f, 0.25f);
			Sensorium.NoteSighting(Enemy, FVector(5, 5, 0), 1.0);
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
