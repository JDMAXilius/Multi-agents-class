// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OSNiagaraParamSetAsset.generated.h"

class UOSNiagaraParamSet;
class UOSNiagaraParamSet_Generic;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSNiagaraParamSetAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TObjectPtr<UOSNiagaraParamSet> Params;
};
