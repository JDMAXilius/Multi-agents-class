#include "Team/AIBTeamCoordinator.h"

#include "AIBotModule.h"
#include "Core/AIBBotController.h"
#include "Core/AIBBotManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/AIBWorldQuery.h"

namespace
{
	/** The alliance predicate the board binds under: the game's one authority when a
	 *  world query is registered; NOBODY-ALLIED when none is. AreAllies, not
	 *  !AreEnemies — AreEnemies folds liveness into hostility, and the negation bound
	 *  a dead enemy's claim across teams for the corpse window (teams W-REVIEW 26 Aug). The
	 *  fallback direction matters — "nobody is allied" makes every claim non-binding
	 *  on everyone, so a host that wired nothing gets an inert board, never a
	 *  colluding one. */
	bool ResolveAreAllies(const UWorld* World, const AActor* A, const AActor* B)
	{
		const UAIBBotManager* Manager = World ? World->GetSubsystem<UAIBBotManager>() : nullptr;
		const IAIBWorldQuery* Query = Manager ? Manager->GetWorldQuery() : nullptr;
		if (!Query || !A || !B)
		{
			return false;
		}
		return Query->AreAllies(A, B);
	}
}

bool UAIBTeamCoordinator::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UAIBTeamCoordinator::TryClaim(const AAIBBotController& Claimant,
	const FAIBPointOfInterest& Target, float TtlSeconds)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: claim refused on a client world."));
		return false;
	}

	// AGENTS ARE NEVER CLAIMABLE. A pawn behind a POI means a provider marked a body a
	// slot — a wiring bug that would turn the board into a shared targeting computer.
	if (Cast<APawn>(Target.Actor.Get()))
	{
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s tried to claim a PAWN-backed target — refused (agents are not slots)."),
			*Claimant.GetName());
		return false;
	}

	const double Now = World->GetTimeSeconds();
	Board.Prune(Now); // before the count, or a grant that displaced a corpse logs as nothing
	const int32 LiveBefore = Board.NumLive(Now);
	const bool bHeld = Board.TryClaim(FObjectKey(&Claimant), Claimant.GetPawn(), Target,
		Now, TtlSeconds,
		[World](const AActor* A, const AActor* B) { return ResolveAreAllies(World, A, B); });

	// Proof 3's countable lines: GRANTS and DENIALS, never renewals (a renewal per
	// think would drown the instrument in cadence noise).
	if (bHeld && Board.NumLive(Now) > LiveBefore)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: claim GRANTED to %s (kind %s)."),
			*Claimant.GetName(), *Target.Kind.ToString());
	}
	else if (!bHeld && Target.bClaimableSlot)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: claim DENIED to %s (kind %s — slot spoken for)."),
			*Claimant.GetName(), *Target.Kind.ToString());
	}
	return bHeld;
}

bool UAIBTeamCoordinator::IsClaimedByOtherTeammate(const AAIBBotController& Asker,
	const FAIBPointOfInterest& Target) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	return Board.IsClaimedByOther(FObjectKey(&Asker), Asker.GetPawn(), Target,
		World->GetTimeSeconds(),
		[World](const AActor* A, const AActor* B) { return ResolveAreAllies(World, A, B); });
}

void UAIBTeamCoordinator::ReleaseAll(const AAIBBotController& Claimant)
{
	const int32 Before = Board.Claims.Num();
	Board.ReleaseAll(FObjectKey(&Claimant));
	if (Board.Claims.Num() < Before)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: claims RELEASED for %s (life over)."), *Claimant.GetName());
	}
}

int32 UAIBTeamCoordinator::NumLiveClaims() const
{
	const UWorld* World = GetWorld();
	return World ? Board.NumLive(World->GetTimeSeconds()) : 0;
}
