// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Game/Data/NMGameModeData.h"
#include "NMGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchStateChanged, FName, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaderboardUpdated, const TArray<ANMPlayerState*>&, PlayerStates);

UCLASS()
class NEWMOONS_API ANMGameState : public AGameState
{
	GENERATED_BODY()

public:
	
	ANMGameState();
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Classes")
	TSubclassOf<APawn> DefaultPlayerClass = nullptr;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	FName GetCurrentPhase() const { return CurrentMatchState; }
	
	APlayerState* GetWinner() const { return WinningPlayer;}

	void SetWinner(APlayerState* Winner);
	
	void StartMatchTimer(int32 Duration);
	void UpdateLeaderboard();
	void HandleMatchStateChange(FName NewMatchState);

	void HandleVictory(APlayerController* Winner);
	void HandleDefeat(APlayerController* Loser);

	UPROPERTY(BlueprintAssignable, Category = "Game Phase")
	FOnMatchStateChanged OnMatchStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnLeaderboardUpdated OnLeaderboardUpdated;

	int32 GetRemainingTime() const;

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly)
	int32 RemainingTime;

	FTimerHandle MatchTimerHandle;

	void UpdateTimer();
	
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_CurrentMatchState")
	FName CurrentMatchState;

	UFUNCTION()
	void OnRep_CurrentMatchState();

private:
	APlayerState* WinningPlayer;
};
