// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSFXTypes.h"
#include "Engine/DataAsset.h"
#include "OSAuraTierFXConfig.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class ONSIGHT_API UOSAuraTierFXConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FOSAuraTierParamSet> AuraParams;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FOSAuraTierAudioSet> AuraAudio;

	UFUNCTION(BlueprintCallable)
	UOSNiagaraParamSet* GetParamsForTier(EOSAuraTier Tier) const
	{
		for (const FOSAuraTierParamSet& param : AuraParams)
		{
			if (param.Tier == Tier)
			{
				return param.DefaultParamsAsset ? param.DefaultParamsAsset->Params : param.AuraParams;
			}
		}
		return nullptr;
	}
	
	const FOSAuraTierAudioSet* GetAudioForTier(EOSAuraTier Tier) const
	{
		for (const FOSAuraTierAudioSet& A : AuraAudio)
			if (A.Tier == Tier)
				return &A;
		return nullptr;
	}
};
