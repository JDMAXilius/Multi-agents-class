// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_AbilityCost.generated.h"

/**
 * 
 */
UCLASS()
class ZORANSRESISTANCE_API UMMC_AbilityCost : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()
public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};
