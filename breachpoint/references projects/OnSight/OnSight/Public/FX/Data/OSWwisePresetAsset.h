// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSFXTypes.h"
#include "Engine/DataAsset.h"
#include "OSWwisePresetAsset.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSWwisePresetAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced)
	UOSWwisePreset* Preset;
};
