#include "Core/OSGameMode.h"
#include "Core/OSGameInstance.h"
#include "OSLogCategories.h"

#include "EngineUtils.h"
#include "Core/OSHUD.h"
#include "Core/OSGameState.h"
#include "Core/OSPlayerController.h"
#include "Core/OSPlayerState.h"
#include "Characters/Player/OSPlayer.h"
#include "Data/OSHitDamageContext.h"
#include "OnlineSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Actors/OSTeamSpawnZone.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AOSGameMode::AOSGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AOSGameState::StaticClass();
	PlayerControllerClass = AOSPlayerController::StaticClass();
	PlayerStateClass = AOSPlayerState::StaticClass();
	DefaultPawnClass = AOSPlayer::StaticClass();
	HUDClass = AOSHUD::StaticClass();
}

void AOSGameMode::CheckWinCondition()
{
	if (!HasAuthority() || bGameEnded) return;

	AOSPlayerState* winner = nullptr; // can be null. counts as a draw
	if (QueryWinCondition(winner))
	{
		EndGame(winner);
	}
}


void AOSGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// Safety net: Ensure Online Subsystem is initialized before creating PlayerState
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Subsystem)
	{
		Subsystem = IOnlineSubsystem::Get(NAME_None);
		UE_LOG(LogTemp, Warning, TEXT("[OSGameMode] Online Subsystem not initialized, forcing NULL subsystem"));
	}

	Super::InitGame(MapName, Options, ErrorMessage);

#if !UE_BUILD_SHIPPING
	UE_LOG(HookMapSelect, Log, TEXT("[OSGameMode] InitGame - Class: %s | Map: %s | Options: %s"),
		*GetClass()->GetName(), *MapName, *Options);
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Magenta,
			FString::Printf(TEXT("[GameMode] %s"), *GetClass()->GetName()));
#endif
}

void AOSGameMode::InitGameState()
{
	Super::InitGameState();
	
	if (AOSGameState* GS = GetGameState<AOSGameState>())
	{
		ConfigureWinCondition(GS);
		// Takes the Layers Config from GameMode and pushes it to GameState for replication.
		// This way, HUD has access to what layers should be built without having to create GameState blueprints for config.
		GS->ModeLayerClasses = ModeLayerClasses;
		if (MatchDurationSeconds > 0)
			GS->InitMatchTimer(MatchDurationSeconds);
		GS->ForceNetUpdate();
	}
}

void AOSGameMode::PostLogin(APlayerController* NewPlayer)
{
	// Server-only: Randomly assign character type BEFORE spawning pawn
	// This ensures GetDefaultPawnClassForController uses the correct type
	if (HasAuthority())
	{
		if (AOSPlayerState* PS = NewPlayer->GetPlayerState<AOSPlayerState>())
		{
			// Random selection from 3 character types (0, 1, 2)
			const int32 RandomIndex = FMath::RandRange(0, 2);
			EOSCharacterType RandomType = static_cast<EOSCharacterType>(RandomIndex);
			PS->SetSelectedCharacterType(RandomType);
			
			UE_LOG(LogTemp, Log, TEXT("[CharacterSelection] Assigned random character type %d to player %s"),
				(int32)RandomType, *PS->GetPlayerName());
		}
	}
	
	// Now call Super to spawn the pawn (which will use GetDefaultPawnClassForController)
	Super::PostLogin(NewPlayer);

	// Start match timer on first player join
	if (AOSGameState* GS = GetGameState<AOSGameState>())
	{
		GS->StartMatchTimer();
	}

	auto playChar = Cast<AOSCharacter>(NewPlayer->GetCharacter());
	if (!playChar) return;

	InitializePlayerState(NewPlayer);
}

AActor* AOSGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Prefer base logic first (handles existing override rules, tags, etc).
	if (AActor* Start = Super::ChoosePlayerStart_Implementation(Player))
	{
		return Start;
	}

	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);

	if (PlayerStarts.Num() <= 0)
	{
		return nullptr;
	}

	const int32 Index = FMath::RandRange(0, PlayerStarts.Num() - 1);
	return PlayerStarts.IsValidIndex(Index) ? PlayerStarts[Index] : nullptr;
}

bool AOSGameMode::QueryWinCondition( AOSPlayerState*& OutWinner ) const
{
	OutWinner = nullptr;
	return false;
}

void AOSGameMode::EndGame( AOSPlayerState* winnerPlayerState )
{
	if (!HasAuthority() || bGameEnded) return;
	bGameEnded = true;

	const FString WinnerName = winnerPlayerState ? winnerPlayerState->GetPlayerName() : TEXT("None (team win or draw)");
	UE_LOG(LogTemp, Log, TEXT("[OSGameMode] EndGame — Winner: %s. Returning to menu in %.1fs."), *WinnerName, PostMatchRematchDelaySeconds);

	AOSGameState* GS = GetGameState<AOSGameState>();
	if (GS)
	{
		GS->StopMatchTimer();
		BuildMatchResult(GS, winnerPlayerState);
	}

	// Clear any in-flight respawn timers so they don't fire during the post-match
	// rematch delay (would otherwise spawn ghost pawns between EndGame and ServerTravel).
	ClearAllRespawnTimers();

	// Return to main menu after delay — routes through EndMatchAndReturnToMenu (EndSession → DestroySession → ServerTravel).
	GetWorldTimerManager().ClearTimer(RestartTimerHandle);
	GetWorldTimerManager().SetTimer(RestartTimerHandle, this,
		&AOSGameMode::RestartCurrentMatch,
		PostMatchRematchDelaySeconds, false);
}

void AOSGameMode::ClearAllRespawnTimers()
{
	for (auto& Pair : RespawnTimers)
	{
		GetWorldTimerManager().ClearTimer(Pair.Value);
	}
	RespawnTimers.Empty();
}

void AOSGameMode::BuildMatchResult( AOSGameState* GS, AOSPlayerState* PS )
{
	GS->MatchResult.bGameEnded = true;
	GS->MatchResult.Winner = PS;
	GS->ForceNetUpdate();
	GS->OnRep_MatchResult();
}

void AOSGameMode::OnMatchTimeExpired()
{
	if (bGameEnded)
		return;

	AOSGameState* GS = GetGameState<AOSGameState>();
	AOSPlayerState* Winner = GS ? GS->MatchLeader.Leader.Get() : nullptr;

	if (!Winner && GS)
	{
		Winner = FindMatchLeaderFallback(GS);
	}

	EndGame(Winner);
}

AOSPlayerState* AOSGameMode::FindMatchLeaderFallback(const AOSGameState* GS) const
{
	AOSPlayerState* Best = nullptr;
	int32 BestValue = -1;

	for (APlayerState* PS : GS->PlayerArray)
	{
		AOSPlayerState* OSPS = Cast<AOSPlayerState>(PS);
		if (!OSPS) continue;

		int32 Value = 0;
		switch (GS->WinConditionEntry.StatType)
		{
		case FOSWinConditionType::KILLS: Value = OSPS->Stats.Kills; break;
		case FOSWinConditionType::SCORE: Value = OSPS->Stats.Score; break;
		default: continue;
		}

		if (Value > BestValue)
		{
			BestValue = Value;
			Best = OSPS;
		}
	}

	return Best;
}

void AOSGameMode::RestartCurrentMatch()
{
	if (!HasAuthority()) return;

	UE_LOG(LogTemp, Log, TEXT("[OSGameMode] RestartCurrentMatch — triggering EndMatchAndReturnToMenu"));
	if (UOSGameInstance* GI = GetGameInstance<UOSGameInstance>())
	{
		GI->EndMatchAndReturnToMenu();
	}
}

void AOSGameMode::HandlePlayerDeath(AController* VictimController, const FOSDeathEventInfo& DeathEvent)
{
	if (!HasAuthority() || !IsValid(VictimController))
	{
		return;
	}

	// Resolve instigator from UniqueId for FFA scoring
	APlayerState* InstigatorPS = nullptr;
	APlayerState* VictimPS = VictimController->GetPlayerState<APlayerState>();
	
	if (DeathEvent.InstigatorPlayerStateUniqueId.IsValid())
	{
		InstigatorPS = DeathEvent.GetInstigatorPlayerState(GetWorld());
		if (InstigatorPS)
		{
			// Only award kill if instigator is different from victim (no suicide)
			if (InstigatorPS != VictimPS)
			{
				// Update FFA scoring (kills/deaths)
				if (AOSPlayerState* OSInstigatorPS = Cast<AOSPlayerState>(InstigatorPS))
				{
					const int32 NewKills = OSInstigatorPS->Stats.Kills + 1;
					OSInstigatorPS->UpdateKillScore(NewKills);
					
					FString InstigatorName = InstigatorPS->GetPlayerName();
					int32 InstigatorKills = OSInstigatorPS->Stats.Kills;
					
					UE_LOG(LogTemp, Log, TEXT("[FFA] Kill registered. Killer=%s (Kills=%d) Victim=%s"),
						*InstigatorName, InstigatorKills, *GetNameSafe(VictimPS));
					/*
					// Print to screen
					if (GEngine)
					{
						GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
							FString::Printf(TEXT("KILL: %s killed %s (Kills: %d)"), 
								*InstigatorName, 
								VictimPS ? *VictimPS->GetPlayerName() : TEXT("Unknown"),
								InstigatorKills));
					}
					*/
				}
			}
		}
	}
	
	AOSPlayerState* OSVictimPS = Cast<AOSPlayerState>(VictimPS);
	// Update victim deaths (skip for manual respawns)
	if (OSVictimPS && !DeathEvent.bIsManualRespawn)
	{
		const int32 NewDeaths = OSVictimPS->Stats.Deaths + 1;
		OSVictimPS->UpdateDeathScore(NewDeaths);
		int32 VictimDeaths = OSVictimPS->Stats.Deaths;

		FString VictimName = VictimPS ? VictimPS->GetPlayerName() : TEXT("Unknown");

		UE_LOG(LogTemp, Log, TEXT("[FFA] Death registered. Victim=%s (Deaths=%d)"),
			*VictimName, VictimDeaths);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Respawn] Death confirmed. Controller=%s Pawn=%s (queued respawn)"),
		*GetNameSafe(VictimController),
		*GetNameSafe(VictimController->GetPawn()));


	if (auto gs = Cast<AOSGameState>(GameState))
		gs->AddKillfeed_Server(DeathEvent);
	QueueRespawn(VictimController);
}


void AOSGameMode::ConfigureWinCondition( AOSGameState* GS )
{
	GS->WinConditionEntry.StatType   = WinCondition.StatType;
	GS->WinConditionEntry.StatTarget = WinCondition.StatTarget;
}

void AOSGameMode::UpdateWinCondition()
{
	if (GrabGameState())
		_GS->UpdateWinCondition_Server(WinCondition.StatType, WinCondition.StatTarget);
}

AOSGameState* AOSGameMode::GrabGameState()
{
	if (IsValid(_GS)) return _GS;
	_GS = Cast<AOSGameState>(GameState);
	return _GS;
}

void AOSGameMode::QueueRespawn(AController* VictimController)
{
	if (!IsValid(VictimController)) return;
	if (bGameEnded) return; // No respawns after match end (FFA lacked this guard).

	AOSPlayerState* PS = VictimController->GetPlayerState<AOSPlayerState>();
	const float RespawnDelaySeconds = CalculateRespawnDelay(PS);

	if (PS)
	{
		PS->SetRespawnCountdown(RespawnDelaySeconds);
	}

	// Clear any stale timer for this controller before overwriting the map entry.
	// Second death before first respawn previously orphaned the first timer in FTimerManager.
	if (FTimerHandle* Existing = RespawnTimers.Find(VictimController))
	{
		GetWorldTimerManager().ClearTimer(*Existing);
	}

	FTimerHandle TimerHandle;
	RespawnTimers.Add(VictimController, TimerHandle);

	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &AOSGameMode::PerformRespawn, VictimController);

	GetWorldTimerManager().SetTimer(
		RespawnTimers[VictimController],
		Delegate,
		RespawnDelaySeconds,
		false
	);

	UE_LOG(LogTemp, Verbose, TEXT("[Respawn] Scheduled. Controller=%s FireIn=%.2fs"),
		*GetNameSafe(VictimController),
		RespawnDelaySeconds);
}

void AOSGameMode::InitializePlayerState( const APlayerController* PlayerController) const
{
	if (AOSPlayerState* PS = PlayerController->GetPlayerState<AOSPlayerState>())
	{
		PS->SetRandomizedCharacterColor(CharacterColors);
		OnPlayerStateReady.Broadcast(PS);
	}
}

void AOSGameMode::PerformRespawn(AController* VictimController)
{
	if (!HasAuthority() || !IsValid(VictimController)) return;
	if (bGameEnded) return; // Ghost respawn during post-match delay.

	RespawnTimers.Remove(TWeakObjectPtr<AController>(VictimController));

	// Detach from dead pawn BEFORE RestartPlayer.
	// UE's RestartPlayerAtTransform skips spawning a new pawn when GetPawn() != nullptr
	// (GameModeBase.cpp:1338). Without UnPossess, the player "respawns" at their death location.
	APawn* OldPawn = VictimController->GetPawn();
	if (IsValid(OldPawn))
	{
		VictimController->UnPossess();
		OldPawn->SetLifeSpan(2.0f);
	}

	// RestartPlayer checks team spawn zones first, falls back to PlayerStart.
	// Loadout grants: AOSCharacter::InitializeGAS (PossessedBy / OnRep_PlayerState), not GameMode.
	RestartPlayer(VictimController);

	// CRITICAL: Domination's OnPlayerReady handler depends on this broadcast
	// for spawn-inside-CP overlap detection. Must always fire after respawn.
	if (AOSPlayerState* PS = VictimController->GetPlayerState<AOSPlayerState>())
	{
		PS->ClearRespawnCountdown();
		OnPlayerStateReady.Broadcast(PS);
	}
}

// ========================================
// TEAM SPAWN ZONES
// ========================================

void AOSGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!HasAuthority() || !IsValid(NewPlayer)) return;

	if (AOSTeamSpawnZone* Zone = FindBestSpawnZone(NewPlayer))
	{
		const FTransform SpawnXf = Zone->GetSpawnTransform();

		UE_LOG(LogOSSpawn, Log, TEXT("[Spawn] %s -> Zone (Team %d) at %s"),
			*GetNameSafe(NewPlayer), Zone->GetTeamIndex(),
			*SpawnXf.GetLocation().ToString());

		RestartPlayerAtTransform(NewPlayer, SpawnXf);
		return;
	}

	UE_LOG(LogOSSpawn, Log, TEXT("[Spawn] %s -> No zone found, using PlayerStart fallback"),
		*GetNameSafe(NewPlayer));
	Super::RestartPlayer(NewPlayer);
}

AOSTeamSpawnZone* AOSGameMode::FindBestSpawnZone(AController* Player) const
{
	if (!Player) return nullptr;

	const AOSPlayerState* PS = Player->GetPlayerState<AOSPlayerState>();

	// FFA or no team assigned — skip zones, use default PlayerStart path.
	// NoTeam.GetId() == 255 (not INDEX_NONE == -1), so check the FGenericTeamId directly.
	if (!PS || PS->GetGenericTeamId() == FGenericTeamId::NoTeam)
	{
		UE_LOG(LogOSSpawn, Log, TEXT("[Spawn] FindBestSpawnZone: %s has no team (NoTeam), skipping zones"),
			*GetNameSafe(Player));
		return nullptr;
	}

	const int32 PlayerTeam = PS->GetTeamIndex();

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	AOSTeamSpawnZone* Best = nullptr;
	float BestScore = -1.f;
	int32 ZoneCount = 0;

	for (TActorIterator<AOSTeamSpawnZone> It(World); It; ++It)
	{
		AOSTeamSpawnZone* Zone = *It;
		if (!IsValid(Zone)) continue;
		++ZoneCount;

		if (!Zone->ServesTeam(PlayerTeam))
		{
			UE_LOG(LogOSSpawn, Verbose, TEXT("[Spawn] Zone %s (Team %d) doesn't serve Team %d"),
				*Zone->GetName(), Zone->GetTeamIndex(), PlayerTeam);
			continue;
		}

		const float Score = Zone->ScoreForPlayer(Player);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Zone;
		}
	}

	UE_LOG(LogOSSpawn, Log, TEXT("[Spawn] FindBestSpawnZone: %s Team=%d, found %d zones, best=%s (score=%.2f)"),
		*GetNameSafe(Player), PlayerTeam, ZoneCount,
		Best ? *Best->GetName() : TEXT("NONE"), BestScore);

	return Best;
}

float AOSGameMode::CalculateRespawnDelay(const AOSPlayerState* VictimPS) const
{
	if (!VictimPS)
	{
		return RespawnDelay;
	}
	return FMath::Clamp(RespawnDelay + (VictimPS->Stats.Deaths * RespawnPenaltyPerDeath), 0.f, MaxRespawnDelay);
}

UClass* AOSGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// Get character type from PlayerState (replicated, persists through respawn)
	if (AOSPlayerState* PS = InController->GetPlayerState<AOSPlayerState>())
	{
		TSubclassOf<AOSPlayer> SelectedClass = nullptr;
		
		switch (PS->GetSelectedCharacterType())
		{
		case EOSCharacterType::Character1:
			SelectedClass = Character1Class;
			break;
		case EOSCharacterType::Character2:
			SelectedClass = Character2Class;
			break;
		case EOSCharacterType::Character3:
			SelectedClass = Character3Class;
			break;
		default:
			SelectedClass = Character1Class; // Default fallback
			break;
		}
		
		// Return the Blueprint class if set, otherwise fallback to base C++ class
		if (SelectedClass)
		{
			return SelectedClass;
		}
	}

	// Fallback to default if PlayerState not ready yet or Blueprint classes not set
	return DefaultPawnClass;
}

