#include "TDM/OSTDMGameMode.h"

#include "Characters/OSCharacter.h"
#include "Core/OSGameState.h"
#include "Core/OSPlayerState.h"
#include "TDM/OSTDMPlayerStart.h"
#include "Engine/PlayerStartPIE.h"
#include "EngineUtils.h"
#include "GenericTeamAgentInterface.h"
#include "TimerManager.h"
// Team spawn zones handled by AOSGameMode::RestartPlayer (base class).

AOSTDMGameMode::AOSTDMGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AOSGameState::StaticClass();
	PlayerStateClass = AOSPlayerState::StaticClass();
	UE_LOG(LogTemp, Log, TEXT("[TDM] GameMode active. MaxTDMTeams=2. Teams assigned equally via GetLowestPopulationTeamIndex()."));
}

FString AOSTDMGameMode::InitNewPlayer(APlayerController* NewPlayerController,
                                      const FUniqueNetIdRepl& UniqueId,
                                      const FString& Options,
                                      const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	AOSGameState* GS = GetWorld() ? GetWorld()->GetGameState<AOSGameState>() : nullptr;
	AOSPlayerState* PS = IsValid(NewPlayerController)
		? NewPlayerController->GetPlayerState<AOSPlayerState>()
		: nullptr;

	if (IsValid(GS) && IsValid(PS) && PS->GetGenericTeamId() == FGenericTeamId::NoTeam)
	{
		const int32 TeamIndex = GS->GetLowestPopulationTeamIndex();
		PS->SetGenericTeamId(FGenericTeamId(static_cast<uint8>(TeamIndex)));

		// Log team assignment and current counts for testing (2 teams, equal distribution)
		TArray<int32> Counts;
		Counts.SetNumZeroed(AOSGameState::MaxTDMTeams);
		for (int32 i = 0; i < GS->PlayerArray.Num(); ++i)
		{
			if (const AOSPlayerState* Other = Cast<AOSPlayerState>(GS->PlayerArray[i]))
			{
				const int32 T = Other->GetTeamIndex();
				if (T >= 0 && T < AOSGameState::MaxTDMTeams)
				{
					Counts[T]++;
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[TDM] Team assigned: Player=\"%s\" -> Team %d (Team0=%d, Team1=%d)"),
			*PS->GetPlayerName(), TeamIndex, Counts[0], Counts[1]);
	}

	return Result;
}

void AOSTDMGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Log full roster after each join for TDM testing (2 teams)
	AOSGameState* GS = GetWorld() ? GetWorld()->GetGameState<AOSGameState>() : nullptr;
	if (!HasAuthority() || !IsValid(GS)) return;

	TArray<int32> TeamCounts;
	TeamCounts.SetNumZeroed(AOSGameState::MaxTDMTeams);
	UE_LOG(LogTemp, Log, TEXT("[TDM] ---- Roster (Team 0 vs Team 1) ----"));
	for (int32 i = 0; i < GS->PlayerArray.Num(); ++i)
	{
		if (const AOSPlayerState* P = Cast<AOSPlayerState>(GS->PlayerArray[i]))
		{
			const int32 T = P->GetTeamIndex();
			if (T >= 0 && T < AOSGameState::MaxTDMTeams)
			{
				TeamCounts[T]++;
				UE_LOG(LogTemp, Log, TEXT("[TDM]   \"%s\" -> Team %d"), *P->GetPlayerName(), T);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[TDM]   \"%s\" -> No team (index=%d)"), *P->GetPlayerName(), T);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[TDM]   Total: Team0=%d, Team1=%d ----"), TeamCounts[0], TeamCounts[1]);
}

// Pushes MatchConfig.KillLimit/TimeLimitSeconds to GameState (TDMKillLimit, TDMTimeLimitSeconds) for client HUD.
// Starts match timer only when TimeLimitSeconds > 0. End conditions: ReportKill (kill limit) and OnMatchTimeExpired (time); whichever is met first ends the match.
void AOSTDMGameMode::InitGameState()
{
	Super::InitGameState();
	if (HasAuthority())
	{
		AOSGameState* GS = GetGameState<AOSGameState>();
		if (GS)
		{
			GS->TDMKillLimit = MatchConfig.KillLimit;
			GS->TDMTimeLimitSeconds = MatchDurationSeconds;
		}
		UE_LOG(LogTemp, Log, TEXT("[TDM] GameState ready. 2 teams (Team 0 vs Team 1). Use this map with TDM GameMode to test."));
	}
}

void AOSTDMGameMode::ConfigureWinCondition(AOSGameState* GS)
{
	Super::ConfigureWinCondition(GS);
	if (!GS || !HasAuthority()) return;
	GS->UpdateWinCondition_Server(FOSWinConditionType::KILLS, MatchConfig.KillLimit);
}

void AOSTDMGameMode::HandlePlayerDeath(AController* VictimController, const FOSDeathEventInfo& DeathEvent)
{
	if (!HasAuthority() || !IsValid(VictimController))
	{
		return;
	}

	if (!DeathEvent.bIsManualRespawn)
	{
		AController* KillerController = DeathEvent.GetInstigatorController(GetWorld());
		ReportKill(KillerController, VictimController, nullptr);

		if (AOSGameState* GS = GetGameState<AOSGameState>())
		{
			GS->AddKillfeed_Server(DeathEvent);
		}
	}

	// Do not respawn after match is over.
	AOSGameState* GS = GetGameState<AOSGameState>();
	if (GS && !GS->IsMatchOver())
	{
		RequestRespawn(VictimController);
	}
}

void AOSTDMGameMode::ReportKill(AController* KillerController,
                                AController* VictimController,
                                AController* AssistController)
{
	if (!HasAuthority()) return;

	AOSGameState* GS = GetWorld() ? GetWorld()->GetGameState<AOSGameState>() : nullptr;
	if (!IsValid(GS) || GS->IsMatchOver()) return;

	AOSPlayerState* VictimPS = IsValid(VictimController) ? VictimController->GetPlayerState<AOSPlayerState>() : nullptr;
	AOSPlayerState* KillerPS = IsValid(KillerController) ? KillerController->GetPlayerState<AOSPlayerState>() : nullptr;
	AOSPlayerState* AssistPS = IsValid(AssistController) ? AssistController->GetPlayerState<AOSPlayerState>() : nullptr;

	const int32 VictimTeam = VictimPS ? VictimPS->GetTeamIndex() : -1;
	const int32 KillerTeam = KillerPS ? KillerPS->GetTeamIndex() : -1;

	if (IsValid(VictimController))
	{
		if (VictimPS)
		{
			VictimPS->AddDeath();
			if (VictimTeam >= 0) GS->AddTeamDeath(VictimTeam);
		}
	}

	// TDM rule: only award kills/assists/team score for true enemy kills.
	// Suicides and friendly fire still count as deaths for the victim, but do not
	// grant kill credit, assist credit, or team score.
	const bool bHasKiller = IsValid(KillerPS);
	const bool bValidEnemyKill = bHasKiller && KillerPS != VictimPS && KillerTeam >= 0 && VictimTeam >= 0 && KillerTeam != VictimTeam;

	if (bValidEnemyKill)
	{
		KillerPS->AddKill();
		GS->AddTeamKill(KillerTeam);
		// Kill limit: first team to reach KillLimit wins (no-op if KillLimit == 0).
		if (MatchConfig.KillLimit > 0 && GS->GetTeamKills(KillerTeam) >= MatchConfig.KillLimit)
		{
			UE_LOG(LogTemp, Log, TEXT("[TDM] Kill limit reached — Team %d wins. Ending match."), KillerTeam);
			GS->NotifyMatchWon(KillerTeam);
			EndGame(nullptr);  // sets bGameEnded, stops timer, builds result, schedules EndMatchAndReturnToMenu
		}
		if (IsValid(AssistPS) && AssistPS != KillerPS && AssistPS != VictimPS)
		{
			AssistPS->AddAssist();
		}
	}
	else if (bHasKiller)
	{
		UE_LOG(LogTemp, Log, TEXT("[TDM] Non-scoring death: KillerTeam=%d VictimTeam=%d (suicide or friendly fire). No kill/assist/team score awarded."),
			KillerTeam, VictimTeam);
	}

	// [TDM] Kill log for testing
	{
		const FString VictimName = IsValid(VictimController)
			? (VictimController->GetPlayerState<AOSPlayerState>() ? VictimController->GetPlayerState<AOSPlayerState>()->GetPlayerName() : TEXT("?"))
			: TEXT("?");
		const int32 VictimTeamTemp = IsValid(VictimController) && VictimController->GetPlayerState<AOSPlayerState>()
			? VictimController->GetPlayerState<AOSPlayerState>()->GetTeamIndex()
			: -1;
		const FString KillerName = IsValid(KillerController)
			? (KillerController->GetPlayerState<AOSPlayerState>() ? KillerController->GetPlayerState<AOSPlayerState>()->GetPlayerName() : TEXT("?"))
			: TEXT("(none)");
		const int32 T0 = GS->GetTeamKills(0);
		const int32 T1 = GS->GetTeamKills(1);
		if (bValidEnemyKill)
		{
			UE_LOG(LogTemp, Log, TEXT("[TDM] Kill: %s (Team %d) killed %s (Team %d). Scores: Team0=%d, Team1=%d"),
				*KillerName, KillerTeam, *VictimName, VictimTeamTemp, T0, T1);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[TDM] Death: %s (Team %d) died. Scores: Team0=%d, Team1=%d"),
				*VictimName, VictimTeamTemp, T0, T1);
		}
	}
}

void AOSTDMGameMode::RequestRespawn(AController* VictimController)
{
	if (!IsValid(VictimController)) return;

	AOSGameState* GS = GetGameState<AOSGameState>();
	if (GS && GS->IsMatchOver()) return;

	if (APawn* DeadPawn = VictimController->GetPawn())
	{
		VictimController->UnPossess();
		DeadPawn->SetLifeSpan(2.0f);
	}

	QueueRespawn(VictimController);
}

AActor* AOSTDMGameMode::ChoosePlayerStart_Implementation(AController* Player)
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

	const AOSPlayerState* PS = Player->GetPlayerState<AOSPlayerState>();
	const int32 PlayerTeam = PS ? PS->GetTeamIndex() : -1;

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

		if (Start->TeamIndex != -1 && Start->TeamIndex != PlayerTeam)
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

void AOSTDMGameMode::RestartPlayer(AController* NewPlayer)
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

void AOSTDMGameMode::OnMatchTimeExpired()
{
	AOSGameState* GS = GetGameState<AOSGameState>();
	if (!GS || GS->IsMatchOver()) return;  // Already ended (e.g. kill limit); time and kill are "whichever first".

	const int32 T0 = GS->GetTeamKills(0);
	const int32 T1 = GS->GetTeamKills(1);
	int32 WinningTeam = -1;
	if (T0 > T1)
	{
		WinningTeam = 0;
	}
	else if (T1 > T0)
	{
		WinningTeam = 1;
	}
	// else tied: WinningTeam stays -1 (draw)

	UE_LOG(LogTemp, Log, TEXT("[TDM] Time limit reached. Team0=%d, Team1=%d. %s"),
		T0, T1, WinningTeam >= 0 ? (WinningTeam == 0 ? TEXT("Team 0 wins.") : TEXT("Team 1 wins.")) : TEXT("Draw."));
	GS->NotifyMatchWon(WinningTeam);
	EndGame(nullptr);  // sets bGameEnded, stops timer, builds result, schedules EndMatchAndReturnToMenu
}

