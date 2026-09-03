#include "Team/AIBTeamCoordinator.h"

#include "AIBotModule.h"
#include "Core/AIBBotController.h"
#include "Core/AIBBotManager.h"
#include "Core/AIBTypes.h"
#include "Data/AIBDataRows.h"
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

	/** Phase 12 death release: AreEnemies folds liveness ("a corpse is nobody's enemy") —
	 *  the injected door, no GAS. Without a query nothing can say "dead", so nothing is. */
	bool ResolveIsLiveEnemy(const UWorld* World, const AActor* ClaimantPawn, const AActor* Target)
	{
		const UAIBBotManager* Manager = World ? World->GetSubsystem<UAIBBotManager>() : nullptr;
		const IAIBWorldQuery* Query = Manager ? Manager->GetWorldQuery() : nullptr;
		return !Query || !ClaimantPawn || Query->AreEnemies(ClaimantPawn, Target);
	}

	const TCHAR* ReleaseReason(EAIBTargetClaimRelease Reason)
	{
		switch (Reason)
		{
		case EAIBTargetClaimRelease::Exit:      return TEXT("exit");
		case EAIBTargetClaimRelease::Death:     return TEXT("death");
		case EAIBTargetClaimRelease::Unpossess: return TEXT("unpossess");
		default:                                return TEXT("ttl");
		}
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

	// A pawn behind a POI means a provider marked a body a SLOT — a wiring bug. Agents are
	// claimable since Phase 12, but through TryClaimTarget's capped book, never this one.
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
	// Phase 12: the same belt for target claims (reason=unpossess), logged per claim.
	TArray<FAIBReleasedTargetClaim> Released;
	TargetClaims.ReleaseAll(FObjectKey(&Claimant), Released);
	LogReleases(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, Released);
}

int32 UAIBTeamCoordinator::NumLiveClaims() const
{
	const UWorld* World = GetWorld();
	return World ? Board.NumLive(World->GetTimeSeconds()) : 0;
}

// ---------------------------------------------------------------- Phase 12: target claims

void UAIBTeamCoordinator::LogReleases(double Now, const TArray<FAIBReleasedTargetClaim>& Released) const
{
	for (const FAIBReleasedTargetClaim& R : Released)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f target claim RELEASED on %s reason=%s"),
			*R.ClaimantName, Now, *R.TargetName, ReleaseReason(R.Reason));
	}
}

void UAIBTeamCoordinator::PruneTargetClaims(double Now)
{
	const UWorld* World = GetWorld();
	TArray<FAIBReleasedTargetClaim> Released;
	TargetClaims.Prune(Now,
		[World](const AActor* ClaimantPawn, const AActor* Target) { return ResolveIsLiveEnemy(World, ClaimantPawn, Target); },
		Released);
	LogReleases(Now, Released);
}

EAIBTargetClaimResult UAIBTeamCoordinator::TryClaimTarget(const AAIBBotController& Claimant, const AActor& Target, int32& OutHolders)
{
	OutHolders = 0;
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: claim refused on a client world."));
		return EAIBTargetClaimResult::Denied;
	}
	const double Now = World->GetTimeSeconds();
	PruneTargetClaims(Now);
	return TargetClaims.TryClaim(FObjectKey(&Claimant), Claimant.GetPawn(), &Target, Now, AIB::ClaimTtlSeconds,
		[World](const AActor* A, const AActor* B) { return ResolveAreAllies(World, A, B); },
		OutHolders, Claimant.GetName(), Target.GetName());
}

int32 UAIBTeamCoordinator::CountAlliesOnTarget(const AAIBBotController& Asker, const AActor& Target) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	return TargetClaims.CountAlliesOn(FObjectKey(&Asker), Asker.GetPawn(), &Target, World->GetTimeSeconds(),
		[World](const AActor* A, const AActor* B) { return ResolveAreAllies(World, A, B); });
}

bool UAIBTeamCoordinator::HoldsTargetClaim(const AAIBBotController& Asker, const AActor& Target) const
{
	const UWorld* World = GetWorld();
	return World && TargetClaims.Holds(FObjectKey(&Asker), &Target, World->GetTimeSeconds());
}

int32 UAIBTeamCoordinator::GetTargetClaimOrdinal(const AAIBBotController& Asker, const AActor& Target) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return INDEX_NONE;
	}
	return TargetClaims.Ordinal(FObjectKey(&Asker), Asker.GetPawn(), &Target, World->GetTimeSeconds(),
		[World](const AActor* A, const AActor* B) { return ResolveAreAllies(World, A, B); });
}

void UAIBTeamCoordinator::ReleaseTargetClaimsOnExit(const AAIBBotController& Claimant, float MinHoldSeconds)
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	PruneTargetClaims(Now);
	TArray<FAIBReleasedTargetClaim> Released;
	TargetClaims.ReleaseOnExit(FObjectKey(&Claimant), Now, MinHoldSeconds, Released);
	LogReleases(Now, Released);
}

// ---------------------------------------------------------------- Phase 12: sightings + heat

void UAIBTeamCoordinator::PublishSighting(const AAIBBotController& Reporter, AActor& Target, const FVector& Where, double SeenAtSeconds)
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	Sightings.Publish(FObjectKey(&Reporter), Reporter.GetPawn(), Reporter.GetName(), &Target, Where, SeenAtSeconds, World->GetTimeSeconds());
}

void UAIBTeamCoordinator::ForEachTeamReport(const AAIBBotController& Asker, float StaleSeconds,
	TFunctionRef<void(const FAIBSighting&)> Visit) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	Sightings.ForEachReport(FObjectKey(&Asker), Asker.GetPawn(), World->GetTimeSeconds(), StaleSeconds,
		[World](const AActor* A, const AActor* B) { return ResolveAreAllies(World, A, B); }, Visit);
}

void UAIBTeamCoordinator::StampVisit(const AAIBBotController& Visitor, const FVector& Where, float CellUU, float DecaySeconds)
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	VisitHeat.Stamp(FObjectKey(&Visitor), Visitor.GetPawn(), Where, Now, CellUU);
	// The once-per-think housekeeping for all three members rides the stamp: every live
	// bot calls it, so a death release is noticed within one think without a tick.
	VisitHeat.Prune(Now, DecaySeconds);
	Sightings.Prune(Now, FMath::Max(Visitor.GetTierRow().TeamReportStaleSeconds, 0.f) + 1.f);
	PruneTargetClaims(Now);
}

float UAIBTeamCoordinator::VisitHeatAt(const AAIBBotController& Asker, const FVector& Where, float CellUU, float DecaySeconds) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	return VisitHeat.HeatAt(FObjectKey(&Asker), Asker.GetPawn(), Where, World->GetTimeSeconds(), CellUU, DecaySeconds,
		[World](const AActor* A, const AActor* B) { return ResolveAreAllies(World, A, B); });
}
