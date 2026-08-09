// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FX/Data/OSFXTypes.h"
#include "FX/Data/OSWwisePresetAsset.h"
#include "FX/ParamSet/OSWwisePreset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OSWwiseBlueprintLibrary.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSWwiseBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="OnSight|Wwise")
	static FOSWwiseParamPack MakeAudioPackFromPreset(
		UOSWwisePresetAsset* Preset,
		const FOSWwiseParamPack& Overrides)
	{
		auto preset = Preset ? Preset->Preset : nullptr;
		auto Out = preset ? Preset->Preset->BasePack : FOSWwiseParamPack{};
		Out.MergeFrom(Overrides);
		return Out;
	}
};
