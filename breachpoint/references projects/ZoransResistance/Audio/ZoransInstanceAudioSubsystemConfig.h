// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ZoransInstanceAudioSubsystemConfig.generated.h"

class UDataTable;

UCLASS(BlueprintType)
class ZORANSRESISTANCE_API UZoransInstanceAudioSubsystemConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FRuntimeFloatCurve FadeInCurve;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FRuntimeFloatCurve FadeOutCurve;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TObjectPtr<UDataTable> MusicTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float MusicVolume;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetFadeVolume(const float Progress, const bool bFadeIn, const float Multiplier);
};
