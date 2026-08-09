// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FX/Data/OSFXTypes.h"
#include "UObject/Object.h"
#include "OSWwisePreset.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class ONSIGHT_API UOSWwisePreset : public UObject
{
	GENERATED_BODY()
	
public:
	// the core pack of the preset. Needs to be valid (either pair or override events not null)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FOSWwiseParamPack BasePack;
	
	// Packs that can be used as overrides for variations. 
	// When called either through a Notify, randomly, or somewhere else, the Preset can provide this Array of packs as overrides for the BasePack.
	// The pack will then attempt to merge with the BasePack, overriding any valid fields for the final result.
	// Useful for things like sound variations in an attack combo, or random variation within a walking cycle.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FOSWwiseParamPack> VariationPacks;
	
	UFUNCTION(BlueprintCallable)
	void MergeWithVariationPack(int index);
	
	// Used for merging. Don't use to directly call upon the pack.
	const FOSWwiseParamPack* GetVariationPack( int32 Index ) const;
};
