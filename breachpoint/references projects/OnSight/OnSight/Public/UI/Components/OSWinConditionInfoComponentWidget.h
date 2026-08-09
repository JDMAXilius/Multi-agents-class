// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OSWinConditionInfoComponentWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class ONSIGHT_API UOSWinConditionInfoComponentWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	
	void UpdateSelf(int32 newScore, int32 targetScore = -1, const FText& newLabel = FText::GetEmpty()) const;
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnUpdateSelfHook(int32 newScore, int32 targetScore = -1, const FText& newLabel = FText::GetEmpty()) const;

protected:

	UPROPERTY(EditAnywhere, Category="OnSight|Setup", meta=(BindWidgetOptional))
	UTextBlock* ScoreNameTextBlock;
	
	UPROPERTY(EditAnywhere, Category="OnSight|Setup", meta=(BindWidgetOptional))
	UTextBlock* ScoreValueTextBlock;
	
	UPROPERTY(EditAnywhere, Category="OnSight|Setup", meta=(BindWidgetOptional))
	UTextBlock* ScoreTargetTextBlock;
	
	UPROPERTY(EditAnywhere, Category="OnSight|Setup", meta=(BindWidgetOptional))
	UProgressBar* ScoreProgressBar;
};
