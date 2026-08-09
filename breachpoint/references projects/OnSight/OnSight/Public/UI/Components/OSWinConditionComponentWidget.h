// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OSWinConditionComponentWidget.generated.h"

class UTextBlock;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSWinConditionComponentWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent)
	void Print(const FString& playerName, const int32 Score);
};
