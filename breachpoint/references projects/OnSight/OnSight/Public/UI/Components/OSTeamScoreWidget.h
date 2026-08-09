// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OSTeamScoreWidget.generated.h"

class UOSWinConditionInfoComponentWidget;

UCLASS()
class ONSIGHT_API UOSTeamScoreWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetScores(int32 FriendlyScore, int32 EnemyScore, int32 Target);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOSWinConditionInfoComponentWidget> W_TeamScore_Friendly;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOSWinConditionInfoComponentWidget> W_TeamScore_Enemy;
};
