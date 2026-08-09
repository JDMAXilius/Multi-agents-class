// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSGameplayEffect.h"
#include "GE_OSAugmentation.generated.h"

/** Base for augment/tier buff GEs. AppliedAugmentations tag container used by subclasses or BP. */
UCLASS()
class ONSIGHT_API UGE_OSAugmentation : public UOSGameplayEffect
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditDefaultsOnly)
	FGameplayTagContainer AppliedAugmentations;
};
