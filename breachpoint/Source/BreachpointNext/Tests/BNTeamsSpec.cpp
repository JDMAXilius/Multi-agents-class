#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"
#include "Match/BNTeams.h"

/**
 * THE ONE TEAM QUERY, pinned. Every assertion below asks the way the game asks — through
 * BNTeams — because that helper is the only sanctioned caller of FGenericTeamId::GetAttitude
 * in game code, and the guard it wraps is the packet's crown jewel: the engine's default
 * attitude solver answers GetAttitude(NoTeam, NoTeam) == Friendly, so an unguarded query
 * would make every FFA player "friendly" and block ALL damage. This file is what turns that
 * silent regression into a red spec.
 *
 * EVERY ENGINE CALL BELOW IS TRANSCRIBED, NOT REMEMBERED (the BNDamageSpec law): the world
 * machinery is BNDamageSpec's own BuildWorld/DestroyWorld, `FGenericTeamId(uint8)` and
 * `GetId()` are BNBotController.cpp's and BRTelemetrySubsystem's, and the guard itself is
 * quoted from Match/BNTeams.h. The pure table rows need no world at all; only the
 * actor-level ladder spawns anything, and it spawns exactly what BNDamageSpec spawns —
 * plain AActors.
 */
BEGIN_DEFINE_SPEC(FBNTeamsSpec, "BreachpointNext.Sim.Teams",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	UWorld* World = nullptr;

	bool BuildWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World)
		{
			AddError(TEXT("UWorld::CreateWorld returned null; no actor can be spawned without a world."));
			return false;
		}

		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		return true;
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

END_DEFINE_SPEC(FBNTeamsSpec)

void FBNTeamsSpec::Define()
{
	// No BeforeEach: the table rows are pure and must stay provably world-free — only the
	// actor-level ladder builds a world, inside its own test. AfterEach tears down whichever
	// case built one and no-ops for the rest.
	AfterEach([this]()
	{
		DestroyWorld();
	});

	It("keeps exactly two teams, so a silent widening is a visible spec change", [this]()
	{
		TestEqual(TEXT("NumTeams"), static_cast<int32>(BNTeams::NumTeams), 2);
	});

	It("refuses friendship to the unassigned — THE guard that keeps FFA damage alive", [this]()
	{
		const FGenericTeamId NoTeam = FGenericTeamId::NoTeam;
		const FGenericTeamId Team0(0);

		// The engine's default solver calls NoTeam-vs-NoTeam Friendly (same id). Unguarded,
		// that answer would refuse every hit of every FFA match; these two rows are the guard.
		TestFalse(TEXT("NoTeam vs NoTeam"), BNTeams::AreFriendly(NoTeam, NoTeam));
		TestFalse(TEXT("NoTeam vs Team0"), BNTeams::AreFriendly(NoTeam, Team0));
		TestFalse(TEXT("Team0 vs NoTeam"), BNTeams::AreFriendly(Team0, NoTeam));
	});

	It("answers real sides honestly: same team friendly, cross team not", [this]()
	{
		const FGenericTeamId Team0(0);
		const FGenericTeamId Team1(1);

		TestTrue(TEXT("Team0 vs Team0"), BNTeams::AreFriendly(Team0, Team0));
		TestTrue(TEXT("Team1 vs Team1"), BNTeams::AreFriendly(Team1, Team1));
		TestFalse(TEXT("Team0 vs Team1"), BNTeams::AreFriendly(Team0, Team1));
		TestFalse(TEXT("Team1 vs Team0"), BNTeams::AreFriendly(Team1, Team0));
	});

	It("answers NOT-friendly for null and self at the actor level", [this]()
	{
		// The null rungs need no world — the ladder answers before any interface is asked.
		TestFalse(TEXT("null vs null"), BNTeams::AreActorsFriendly(nullptr, nullptr));

		if (!BuildWorld())
		{
			return;
		}
		AActor* Actor = World->SpawnActor<AActor>();
		if (!Actor)
		{
			AddError(TEXT("SpawnActor failed."));
			return;
		}

		TestFalse(TEXT("actor vs null"), BNTeams::AreActorsFriendly(Actor, nullptr));
		TestFalse(TEXT("null vs actor"), BNTeams::AreActorsFriendly(nullptr, Actor));
		// Self is NOT-friendly by design: the friendly-fire gate must never eat a player's
		// own grenade, and answering it here keeps that rule out of every caller.
		TestFalse(TEXT("actor vs itself"), BNTeams::AreActorsFriendly(Actor, Actor));
	});

	It("answers NOT-friendly when either side lacks the team interface", [this]()
	{
		if (!BuildWorld())
		{
			return;
		}
		AActor* A = World->SpawnActor<AActor>();
		AActor* B = World->SpawnActor<AActor>();
		if (!A || !B)
		{
			AddError(TEXT("SpawnActor failed."));
			return;
		}

		// A plain AActor implements no IGenericTeamAgentInterface — the honest default for a
		// projectile, a prop, or anything else the combat path might hand this query.
		TestFalse(TEXT("two interface-less actors"), BNTeams::AreActorsFriendly(A, B));
	});
}

/**
 * NAMED GAPS — what this file deliberately does NOT cover, so nobody reads a green run as
 * more than it is:
 *
 * 1. THE AUTHORITY GATE on ABNPlayerState::SetGenericTeamId (refused on a non-authority).
 *    A standalone CreateWorld has no NetDriver, so every spawned actor is ROLE_Authority and
 *    the refusal branch is unreachable here; forcing a client role would need `AActor::SetRole`,
 *    which nothing in this repo has ever compiled. The gate's proof is rung 4 — the ticket's
 *    degenerate cheat test: a client-side TeamId write never replicates up.
 *
 * 2. REPLICATION of TeamId / TeamScores / WinningTeamId, and the OnRep broadcasts. Shared
 *    memory proves nothing about replication (honesty ladder, law 6) — that is the same
 *    rung-4 scenario, asserted in threes.
 *
 * 3. AreActorsFriendly with two REAL team agents. BNPlayerState is the game's team agent and
 *    spawning one drags the full ASC fixture in; the interface-to-id hop it would add is
 *    covered by the pure table plus the interface-less rung above, and the live version is
 *    exactly what the FF-refusal terminal check exercises.
 */

#endif // WITH_DEV_AUTOMATION_TESTS
