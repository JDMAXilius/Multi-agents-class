// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/OSGameMode.h"
#include "OSGameMode_FFA.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API AOSGameMode_FFA : public AOSGameMode
{
	GENERATED_BODY()

protected:
	virtual void HandlePlayerDeath(AController* VictimController, const FOSDeathEventInfo& DeathEvent) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual bool QueryWinCondition(AOSPlayerState*& outWinner) const override;
	virtual void BuildMatchResult(AOSGameState* GS, AOSPlayerState* PS) override;
};
