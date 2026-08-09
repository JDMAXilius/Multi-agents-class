// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OSNiagaraParamSetGeneratorLibrary.generated.h"

class UNiagaraSystem;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSNiagaraParamSetGeneratorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	// UHT constraint: UFUNCTION declarations cannot live inside `#if` blocks — they must exist
	// in all builds, otherwise UHT fails or produces inconsistent reflection data. The BODY is
	// guarded in the .cpp so editor-only includes (AssetTools, IAssetTools) don't bleed into
	// shipping builds. `CallInEditor` is a no-op in shipping builds anyway — the function can
	// never be called from a BP node at runtime.
	UFUNCTION(BlueprintCallable, CallInEditor, Category="OnSight|FX")
	static UObject* GenerateGenericParamSetAsset(UNiagaraSystem* System, const FString& PackagePath, const FString& AssetName, const bool bAssignDefaults);
};
