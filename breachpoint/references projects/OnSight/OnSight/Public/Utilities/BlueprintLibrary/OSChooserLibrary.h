// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OSChooserLibrary.generated.h"

struct FInstancedStruct;
class UChooserTable;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSChooserLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Chooser")
	static UObject* EvaluateChooser_Object(
		UChooserTable* Table,
		const FInstancedStruct& Params);
	
	UFUNCTION(BlueprintCallable, Category="Chooser")
	static TSubclassOf<UObject> EvaluateChooser_Class(
		UChooserTable* Table,
		const FInstancedStruct& Params);
};
