// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSBaseLayerWidget.h"
#include "Components/TextBlock.h"
#include "Interfaces/OSLayerLayoutInterface.h"
#include "UI/Components/OSScoreboardInfoComponentWidget.h"
#include "OSScoreboardLayerWidget.generated.h"

class UOSScoreboardComponent;
class UVerticalBox;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSScoreboardLayerWidget : public UOSBaseLayerWidget
{
	GENERATED_BODY()
public:
	virtual void NativeDestruct() override;
	virtual void RebuildFromGameState() override;
	virtual void UpdateScoreboard(AOSPlayerState* PlayerState, bool ToAdd);

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UOSScoreboardComponent> ScoreboardComponent;
};
