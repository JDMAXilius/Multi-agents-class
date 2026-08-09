// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_WeaponCooldown.generated.h"

/**
 * 
 */
UCLASS()
class ZORANSRESISTANCE_API UMMC_WeaponCooldown : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()
public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};
