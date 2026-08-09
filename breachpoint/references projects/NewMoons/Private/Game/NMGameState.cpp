// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NMGameState.h"
#include "TimerManager.h"
#include "GameFramework/GameMode.h"
#include "Net/UnrealNetwork.h"
#include "Player/NMPlayerState.h"
#include "UI/NMHUDBase.h"

ANMGameState::ANMGameState()
{
	
}

void ANMGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANMGameState, RemainingTime);
	DOREPLIFETIME(ANMGameState, CurrentMatchState);
}

void ANMGameState::SetWinner(APlayerState* Winner)
{
	WinningPlayer = Winner;
}

void ANMGameState::StartMatchTimer(int32 Duration)
{
	RemainingTime = Duration;
	// Clear any existing match timer to avoid conflicts
	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	// Print match timer started message
	if (GEngine)
	{
		FString TimerMessage = FString::Printf(TEXT("Match timer started: %d seconds"), RemainingTime);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TimerMessage);
	}

	GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &ANMGameState::UpdateTimer, 1.0f, true);
}

void ANMGameState::UpdateLeaderboard()
{
	TArray<ANMPlayerState*> PlayerStates;
	for (APlayerState* PS : PlayerArray)
	{
		ANMPlayerState* PlayerState = Cast<ANMPlayerState>(PS);
		if (PlayerState) PlayerStates.Add(PlayerState);
	}

	PlayerStates.Sort([](const ANMPlayerState& A, const ANMPlayerState& B) {
		return A.GetKills() > B.GetKills();
	});

	OnLeaderboardUpdated.Broadcast(PlayerStates);
}

void ANMGameState::HandleMatchStateChange(FName NewMatchState)
{
	CurrentMatchState = NewMatchState;  // Keep local state
	OnMatchStateChanged.Broadcast(NewMatchState);
}

void ANMGameState::HandleVictory(APlayerController* Winner)
{
	if (Winner)
	{
		GetWorldTimerManager().ClearTimer(MatchTimerHandle);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Victory Player!"));
		ANMHUDBase* HUD = Cast<ANMHUDBase>(Winner->GetHUD());
		if (HUD)
		{
			HUD->ShowVictoryUI();
		}
	}
}

void ANMGameState::HandleDefeat(APlayerController* Loser)
{
	if (Loser)
	{
		GetWorldTimerManager().ClearTimer(MatchTimerHandle);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Defeat Player!"));
		ANMHUDBase* HUD = Cast<ANMHUDBase>(Loser->GetHUD());
		if (HUD)
		{
			HUD->ShowDefeatUI();
		}
	}
}

int32 ANMGameState::GetRemainingTime() const
{
	return RemainingTime;
}

void ANMGameState::UpdateTimer()
{
	if (--RemainingTime <= 0)
	{
		GetWorldTimerManager().ClearTimer(MatchTimerHandle);
		SetMatchState(MatchState::WaitingPostMatch);
		OnMatchStateChanged.Broadcast(MatchState::WaitingPostMatch);
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Match time is up!"));
	}
}

void ANMGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Ensure all timers are cleared when the game ends
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void ANMGameState::OnRep_CurrentMatchState()
{
	if (OnMatchStateChanged.IsBound())
	{
		OnMatchStateChanged.Broadcast(CurrentMatchState);
	}
}

