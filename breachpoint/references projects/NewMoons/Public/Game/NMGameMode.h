// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Player/NMPlayerController.h"
#include "Game/Data/NMGameModeData.h"
#include "NMGameMode.generated.h"

class ANMPlayerState;

UCLASS()
class NEWMOONS_API ANMGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	
	ANMGameMode();
	
	virtual void BeginPlay() override;

	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual void SetMatchState(FName NewState);

	void StartCountdown();
	void StartEndMatchCountdown();
	
	void PlayerEliminated(ANMPlayerState* KillerPlayerState, ANMPlayerState* KilledPlayerState);
	
	void CheckMatchEndCondition();
	
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason);

	void GetSpawnLocationAndRotation(ANMPlayerController* NmController, FVector& Vector, FRotator& Rotator);

	virtual void RespawnCharacter(ANMPlayerController* Controller = nullptr);
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

private:
	UPROPERTY(EditDefaultsOnly, Category= "Properties")
	int32 MaxKillsToWin;
	UPROPERTY(EditDefaultsOnly, Category= "Properties")
	int32 CountdownTime;  // Time for countdown before match begins
	UPROPERTY(EditDefaultsOnly, Category= "Properties")
	int32 MatchTime;  // Time for countdown match in progress
	UPROPERTY(EditDefaultsOnly, Category= "Properties")
	int32 EndMatchCountdownTime;

	FTimerHandle CountdownTimerHandle;
	void HandleCountdown();
	FTimerHandle EndMatchTimerHandle;
	void UpdateEndMatchCountdown();

	void SetPlayerVictory(APlayerState* WinningPlayerState);
};
