// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Data/OSPrimaryDataAssets.h"
#include "FX/Emitters/OSFXEmitter.h"
#include "OSFXEventDataAsset.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSFXEventDataAsset : public UOSDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<TObjectPtr<UOSFXEmitter>> Emitters;
	
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("OSFXEvent"), GetFName());
	}
};
