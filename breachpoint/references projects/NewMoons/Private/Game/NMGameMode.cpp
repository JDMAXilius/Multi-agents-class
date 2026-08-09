// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NMGameMode.h"

#include "Game/NMGameState.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Player/NMPlayerState.h"

ANMGameMode::ANMGameMode()
{
	MaxKillsToWin = 10;
	CountdownTime = 10; // 10-second countdown before match starts
	MatchTime = 300;  // 5-minute match timer
	EndMatchCountdownTime = 10;  // Example: 10 seconds before returning to main menu
}

void ANMGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	// Start the game in WaitingToStart state
	SetMatchState(MatchState::WaitingToStart);
}

void ANMGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	
	// Log match started
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Match is now in progress!"));
	UE_LOG(LogTemp, Log, TEXT("Match has started!"));
	
	ANMGameState* NMGameState = GetGameState<ANMGameState>();
	if (NMGameState)
	{
		NMGameState->StartMatchTimer(MatchTime);  
	}
}

void ANMGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	// Start the end match countdown
	StartEndMatchCountdown();
}

void ANMGameMode::SetMatchState(FName NewState)
{
	Super::SetMatchState(NewState);
	// Update the GameState with the new match state
	ANMGameState* NMGameState = GetGameState<ANMGameState>();
	if (NMGameState)
	{
		NMGameState->HandleMatchStateChange(NewState);  // Broadcast state to clients
	}
	FString StateMessage = FString::Printf(TEXT("MatchState changed to: %s"), *NewState.ToString());
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, StateMessage);
	
	if (NewState == MatchState::WaitingToStart)
	{
		StartCountdown();  // Start the countdown before the match
	}
	else if (NewState == MatchState::InProgress)
	{
		HandleMatchHasStarted();  // Match is now in progress
	}
	else if (NewState == MatchState::WaitingPostMatch)
	{
		HandleMatchHasEnded();  // End the match
	}
	if (NewState == FName("Victory") || NewState == FName("Defeat"))
	{
		// Display the End Game Screen

		HandleMatchHasEnded();// End the match
	}
}

void ANMGameMode::StartCountdown()
{
	// Set a countdown before the match officially starts
	SetMatchState(FName("Countdown"));
	
	// Clear any existing timer to avoid overlap
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	
	// Print the countdown start message
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Countdown started!"));

	GetWorldTimerManager().SetTimer(
		CountdownTimerHandle, 
		this, 
		&ANMGameMode::HandleCountdown, 
		1.0f, 
		true
	);
}

void ANMGameMode::StartEndMatchCountdown()
{
	GetWorldTimerManager().SetTimer(
		EndMatchTimerHandle,
		this,
		&ANMGameMode::UpdateEndMatchCountdown,
		1.0f,  // Update every second
		true
	);
}

void ANMGameMode::UpdateEndMatchCountdown()
{
	if (EndMatchCountdownTime <= 0)
	{
		// Time's up! Return to the main menu
		GetWorldTimerManager().ClearTimer(EndMatchTimerHandle);
		UGameplayStatics::OpenLevel(this, FName("MainMenu"));
	}
	else
	{
		// Display the countdown on the screen
		FString CountdownMessage = FString::Printf(
			TEXT("Returning to main menu in %d seconds..."), EndMatchCountdownTime
		);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, CountdownMessage);
		}

		EndMatchCountdownTime--;
	}
}

void ANMGameMode::HandleCountdown()
{
	if (--CountdownTime <= 0)
	{
		GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
		SetMatchState(MatchState::InProgress);  // Transition to InProgress state
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Match started!"));
	}
	else
	{
		FString CountdownMessage = FString::Printf(TEXT("Match starts in: %d seconds"), CountdownTime);
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, CountdownMessage);
		UE_LOG(LogTemp, Log, TEXT("Match starts in: %d seconds"), CountdownTime);
	}
}

void ANMGameMode::SetPlayerVictory(APlayerState* WinningPlayerState)
{
	ANMGameState* NMGameState = GetGameState<ANMGameState>();
	if (NMGameState)
	{
		NMGameState->SetWinner(WinningPlayerState);
		SetMatchState(FName("Victory"));
		APlayerController* WinnerController = Cast<APlayerController>(WinningPlayerState->GetOwner());
		NMGameState->HandleVictory(WinnerController);
	}
	// Now handle defeat for other players
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (PlayerController)
		{
			APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>();

			if (PlayerState && PlayerState != WinningPlayerState)
			{
				// Set Defeat state for non-winning players
				SetMatchState(FName("Defeat"));
				NMGameState->HandleDefeat(PlayerController);
			}
		}
	}
}

void ANMGameMode::PlayerEliminated(ANMPlayerState* KillerPlayerState, ANMPlayerState* KilledPlayerState)
{
	if (KillerPlayerState) {
		KillerPlayerState->UpdateKillScore(1);
	}
	
	if (KilledPlayerState) {
		KilledPlayerState->UpdateDeathScore(1);
	}
	
	if (GEngine)
	{
		// Elimination message: Display who eliminated whom
		FString EliminationMessage = FString::Printf(
			TEXT("Player: %s eliminated Player: %s!"),
			*KillerPlayerState->GetPlayerName(),
			*KilledPlayerState->GetPlayerName()
		);
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, EliminationMessage);

		// Elimination stats: Show the number of kills for both players
		FString EliminationNumber = FString::Printf(
			TEXT("Killer Player Kills: %d | Victim Player Kills: %d"),
			KillerPlayerState->GetKills(),
			KilledPlayerState->GetKills()
		);
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, EliminationNumber);
	}
	UE_LOG(LogTemp, Log, TEXT("Killer: %s"), *KillerPlayerState->GetPlayerName());
	UE_LOG(LogTemp, Log, TEXT("Victim: %s"), *KilledPlayerState->GetPlayerName());

	CheckMatchEndCondition();
}

void ANMGameMode::CheckMatchEndCondition()
{
	for (APlayerState* PS : GameState->PlayerArray) {
		ANMPlayerState* FFAPlayerState = Cast<ANMPlayerState>(PS);
		if (FFAPlayerState && FFAPlayerState->GetKills() >= MaxKillsToWin) {
			//EndMatch();
			SetPlayerVictory(FFAPlayerState);
			//SetMatchState(MatchState::WaitingPostMatch);
			return;
		}
	}
}

void ANMGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Ensure all timers are cleared when the game ends
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void ANMGameMode::GetSpawnLocationAndRotation(ANMPlayerController* NmController, FVector& Vector, FRotator& Rotator)
{
	if(!NmController){return;}
	
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), Actors);

	// Get the total number of items in the ServerListView
	int32 TotalItems = Actors.Num();
    
	// Check if there are any items in the list
	if (TotalItems == 0) return;

	// Select a random index within the range of available items
	int32 RandomIndex = FMath::RandRange(0, TotalItems - 1);
	
	APlayerStart* PlayerStart = Cast<APlayerStart>(Actors[RandomIndex]);
	Vector = PlayerStart->GetActorLocation();
	Rotator = PlayerStart->GetActorRotation();
}

void ANMGameMode::RespawnCharacter(ANMPlayerController* Controller)
{
	// Ensure the Controller is valid and is a player controller
	if (!Controller || !Controller->IsPlayerController()) return;

	// Get the appropriate player class to spawn
	UClass* PawnClass = nullptr;
	if (const ANMGameState* NMGameState = GetGameState<ANMGameState>())
	{
		PawnClass = NMGameState->DefaultPlayerClass;
		if (!PawnClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("No valid class to spawn for the player"));
			return;
		}
	}

	// Get the spawn location
	const AActor* SpawnPoint = ChoosePlayerStart(Controller);
	if (!SpawnPoint) return;
	const FTransform SpawnTransform = IsValid(SpawnPoint)? SpawnPoint->GetActorTransform() : FTransform::Identity;

	// Find a spawn location and rotation
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	GetSpawnLocationAndRotation(Controller, SpawnLocation, SpawnRotation);

	// Spawn the new pawn for the player
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* NewCharacter = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnLocation, SpawnRotation, SpawnParams);
	//APawn* NewCharacter = GetWorld()->SpawnActorDeferred<APawn>(PawnClass, SpawnTransform, Controller, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	// Destroy the old character and possess the new one
	if (NewCharacter)
	{
		if (APawn* OldCharacter = Controller->GetPawn())
		{
			Controller->UnPossess();
			OldCharacter->Destroy();
		}

		Controller->ClientSetRotation(SpawnRotation);
		Controller->Possess(NewCharacter);
	}
}

void ANMGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer); 
	ANMGameState* NMGameState = GetGameState<ANMGameState>();
	if (NMGameState)
	{
		// Ensure the new player gets the current match phase upon joining
		FName CurrentPhase = NMGameState->GetCurrentPhase();

		// Create a message to log the match state synchronization
		FString Message = FString::Printf(
			TEXT("Player %s joined with current match phase: %s"),
			*NewPlayer->GetPlayerState<APlayerState>()->GetPlayerName(),
			*CurrentPhase.ToString()
		);

		// Display the message on the screen for 10 seconds
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, Message);
		}

		// Log the same message to the output log
		UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
	}
	// Spawn the player if necessary
	if (NewPlayer->GetPawn() == nullptr)
	{
		//RestartPlayer(NewPlayer);
	}
}

void ANMGameMode::RestartPlayer(AController* NewPlayer)
{
	if (NewPlayer->GetPawn() == nullptr)
	{
		// Find a spawn location and rotation
		FVector SpawnLocation = FVector::ZeroVector;
		FRotator SpawnRotation = FRotator::ZeroRotator;
		// Cast to ANMPlayerController and send the match state
		ANMPlayerController* NMPlayerController = Cast<ANMPlayerController>(NewPlayer);
		GetSpawnLocationAndRotation(NMPlayerController, SpawnLocation, SpawnRotation);
		// Spawn the new pawn for the player
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		APawn* NewPawn = GetWorld()->SpawnActor<APawn>(DefaultPawnClass, SpawnLocation, SpawnRotation);
		if (NewPawn)
		{
			NewPlayer->Possess(NewPawn);
		}
	}
}