// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DebugCameraController.h"
#include "OSDebugCameraController.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API AOSDebugCameraController : public ADebugCameraController
{
	GENERATED_BODY()
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	APlayerController* GetOriginalController();
};
