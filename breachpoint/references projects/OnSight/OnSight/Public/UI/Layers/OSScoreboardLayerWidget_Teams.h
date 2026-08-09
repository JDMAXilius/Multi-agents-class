// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSScoreboardLayerWidget.h"
#include "OSScoreboardLayerWidget_Teams.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSScoreboardLayerWidget_Teams : public UOSScoreboardLayerWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(meta=(BindWidgetOptional))
	UOSScoreboardComponent* TeamBoard_A;
	
	UPROPERTY(meta=(BindWidgetOptional))
	UOSScoreboardComponent* TeamBoard_B;

	virtual void NativeDestruct() override;
	virtual void RebuildFromGameState() override;
	UOSScoreboardComponent* GetTeamScoreboard(AOSPlayerState* PS) const;
	void OnBoundPlayerTeamAssigned(AOSPlayerState* PS);
	void OnOtherPlayerTeamAssigned(AOSPlayerState* PS);
	
	virtual void UpdateScoreboard(::AOSPlayerState* PlayerState, bool ToAdd) override;
};
