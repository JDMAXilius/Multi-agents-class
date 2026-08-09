// Copyright Zollpa LLC


#include "ZoransResistance/AbilitySystem/ExecutionCalculations/MMC_AbilityCost.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayAbility.h"

float UMMC_AbilityCost::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	if (const UZoransGameplayAbility* Ability = Cast<UZoransGameplayAbility>(Spec.GetContext().GetAbilityInstance_NotReplicated()))
	{
		return Ability->AbilityCost;
	}

	return 0.f;
}
