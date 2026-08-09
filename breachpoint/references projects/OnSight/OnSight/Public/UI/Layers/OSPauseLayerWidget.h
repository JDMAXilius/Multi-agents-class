// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSBaseLayerWidget.h"
#include "OSPauseLayerWidget.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSPauseLayerWidget : public UOSBaseLayerWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void OpenSettings();
	
	UFUNCTION(BlueprintCallable)
	void ClosePauseMenu();
	
	UFUNCTION(BlueprintCallable)
	void Quit();
	
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UUserWidget> SettingsLayerClass;
};
