// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OSGASDeveloperSettings.generated.h"

class UOSFXLibraryDataAsset;
class UOSGameplayEffect;
/**
 * 
 */
UCLASS(Config=Game, DefaultConfig)
class ONSIGHT_API UOSGASSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Config, Category="Default Effects")
	TSoftClassPtr<UOSGameplayEffect> ClashKnockback;
	UPROPERTY(EditDefaultsOnly, Config, Category="Default Effects")
	TSoftClassPtr<UOSGameplayEffect> DeathState;
	UPROPERTY(EditAnywhere, Config, Category="FX")
	TSoftObjectPtr<UOSFXLibraryDataAsset> DefaultLibrary;
	
};
