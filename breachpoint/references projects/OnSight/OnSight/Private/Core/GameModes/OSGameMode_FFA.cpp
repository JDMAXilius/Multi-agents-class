// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameModes/OSGameMode_FFA.h"

#include "EngineUtils.h"
#include "Core/OSGameState.h"
#include "Core/OSPlayerState.h"
#include "TDM/OSTDMPlayerStart.h"
#include "Engine/PlayerStartPIE.h"


void AOSGameMode_FFA::HandlePlayerDeath( AController* VictimController, const FOSDeathEventInfo& DeathEvent )
{
	if (!HasAuthority() || !IsValid(VictimController)) return;

	// Early UnPossess to match TDM/Domination mode-owned respawn flow (CL 1856). Without this,
	// the base path's `PerformRespawn` UnPossess fires ~3s later via timer, leaving the dying
	// pawn possessed on both server AND owning client during the entire respawn delay. That
	// timing leaks the client's ASC avatar binding to the dead pawn — on respawn, the client's
	// ASC still references the destroyed old pawn, `showdebug abilitysystem` can't find the
	// target, and `TryActivateAbilitiesByTag` fails because the avatar linkage is stale.
	//
	// CL 1856 fixed this for Domination by moving to mode-owned `RequestRespawn`. FFA was not
	// ported in that CL and still exhibited the bug. Early UnPossess here mirrors TDM's
	// `RequestRespawn(VictimController)` behavior without needing a new method on FFA.
	//
	// Must run BEFORE Super::HandlePlayerDeath because Super → QueueRespawn → PerformRespawn
	// would otherwise see the late UnPossess path.
	if (APawn* DeadPawn = VictimController->GetPawn())
	{
		VictimController->UnPossess();
		DeadPawn->SetLifeSpan(2.0f);
	}

	Super::HandlePlayerDeath(VictimController, DeathEvent);

	UpdateWinCondition();
	CheckWinCondition();

	if (auto player = Cast<AOSPlayerState>(DeathEvent.GetInstigatorPlayerState(GetWorld())))
	{
		if (auto gs = GrabGameState())
			gs->TryUpdateMatchLeader_Server(player);
	}
}

AActor* AOSGameMode_FFA::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!IsValid(Player))
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const UClass* PawnClass = GetDefaultPawnClassForController(Player);
	const APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;

	AOSTDMPlayerStart* FoundStart = nullptr;
	AOSTDMPlayerStart* HighestScoreStart = nullptr;
	float HighestScoreValue = -1.f;

	TArray<AOSTDMPlayerStart*> HighScoreStarts;
	TArray<AOSTDMPlayerStart*> OccupiedStarts;

	for (TActorIterator<AOSTDMPlayerStart> It(World); It; ++It)
	{
		AOSTDMPlayerStart* Start = *It;
		if (!IsValid(Start))
		{
			continue;
		}

		if (Start->IsA<APlayerStartPIE>())
		{
			FoundStart = Start;
			break;
		}

		// FFA only uses neutral/any starts.
		if (Start->TeamIndex != -1)
		{
			continue;
		}

		FVector SpawnLoc = Start->GetActorLocation();
		const FRotator SpawnRot = Start->GetActorRotation();

		if (!PawnToFit || !World->EncroachingBlockingGeometry(PawnToFit, SpawnLoc, SpawnRot))
		{
			const float Score = Start->GetSpawnScore(Player);
			if (Score > (1700.f * 1700.f))
			{
				HighScoreStarts.Add(Start);
			}

			if (!IsValid(HighestScoreStart) || Score > HighestScoreValue)
			{
				HighestScoreStart = Start;
				HighestScoreValue = Score;
			}
		}
		else if (PawnToFit && World->FindTeleportSpot(const_cast<APawn*>(PawnToFit), SpawnLoc, SpawnRot))
		{
			OccupiedStarts.Add(Start);
		}
	}

	if (!IsValid(FoundStart))
	{
		if (!HighScoreStarts.IsEmpty())
		{
			FoundStart = HighScoreStarts[FMath::RandRange(0, HighScoreStarts.Num() - 1)];
		}
		else if (IsValid(HighestScoreStart))
		{
			FoundStart = HighestScoreStart;
		}
		else if (!OccupiedStarts.IsEmpty())
		{
			FoundStart = OccupiedStarts[FMath::RandRange(0, OccupiedStarts.Num() - 1)];
		}
	}

	if (IsValid(FoundStart))
	{
		return FoundStart;
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

void AOSGameMode_FFA::RestartPlayer(AController* NewPlayer)
{
	if (!HasAuthority() || !IsValid(NewPlayer))
	{
		return;
	}

	if (AActor* Start = ChoosePlayerStart(NewPlayer))
	{
		RestartPlayerAtPlayerStart(NewPlayer, Start);
		return;
	}

	Super::RestartPlayer(NewPlayer);
}

bool AOSGameMode_FFA::QueryWinCondition( AOSPlayerState*& outWinner ) const
{
	outWinner = nullptr;

	for (TActorIterator<AOSPlayerState> iter(GetWorld()); iter; ++iter)
	{
		AOSPlayerState* PS = *iter;
		if (!PS) continue;
		
		const int32 kills = PS->Stats.Kills;

		if (kills >= WinCondition.StatTarget)
		{
			outWinner = PS;
			return true;
		}
	}

	return false;
}

void AOSGameMode_FFA::BuildMatchResult( AOSGameState* GS, AOSPlayerState* PS )
{
	if (PS)
	{
		GS->MatchResult.WinnerValue = PS->Stats.Kills;
	}
	else
	{
		// Time-expired draw: no winner. WinnerValue = -1 signals draw (matches TDM pattern
		// where NotifyMatchWon(-1) means draw). UI should check for -1 / null Winner.
		GS->MatchResult.WinnerValue = -1;
	}
	Super::BuildMatchResult(GS, PS);
}


