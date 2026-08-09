// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSAbilityGenericResourceCost.generated.h"

/** Instant GE: Additive modifiers on Health/Aura/Stamina via SetByCaller Data.Cost.*. Default for UOSGameplayAbility::GenericCostGE (#97). */
UCLASS()
class ONSIGHT_API UGE_OSAbilityGenericResourceCost : public UOSGameplayEffect
{
	GENERATED_BODY()

	UGE_OSAbilityGenericResourceCost(const FObjectInitializer& ObjectInitializer);
};
