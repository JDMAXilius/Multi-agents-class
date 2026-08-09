// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FX/Data/OSFXTypes.h"
#include "UObject/Object.h"
#include "OSFXEmitter.generated.h"
/**
 * 
 */

UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class ONSIGHT_API UOSFXEmitter : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EOSFXSpawnSpace SpawnSpace = EOSFXSpawnSpace::World;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bEnabled = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagQuery RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagQuery BlockedTags;
	
	// is tracked by FX component
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bPersistent = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="bPersistent==true", EditConditionHides))
	FGameplayTag SlotTag;	
	
	UFUNCTION(BlueprintCallable)
	bool CheckTags(const FGameplayTagContainer& Tags) const
	{
		if (!RequiredTags.IsEmpty() && !RequiredTags.Matches(Tags)) return false;
		if (!BlockedTags.IsEmpty() && BlockedTags.Matches(Tags)) return false;
		return true;
	}
	
	// blueprint handle
	UFUNCTION(BlueprintNativeEvent)
	void Emit(const FOSFXContext& Ctx) const;
	
	virtual void Emit_Implementation(const FOSFXContext& Ctx) const {}
};
