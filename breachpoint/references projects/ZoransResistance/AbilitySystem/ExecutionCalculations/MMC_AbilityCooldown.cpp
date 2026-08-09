// Copyright Zollpa LLC


#include "ZoransResistance/AbilitySystem/ExecutionCalculations/MMC_AbilityCooldown.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayAbility.h"


float UMMC_AbilityCooldown::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	if (const UZoransGameplayAbility* Ability = Cast<UZoransGameplayAbility>(Spec.GetContext().GetAbilityInstance_NotReplicated()))
	{
		const float CooldownMagnitude = Ability->AbilityCooldown;
		return FMath::Clamp<float>(CooldownMagnitude, 0.01f, CooldownMagnitude);
	}

	return 0.01f;
}
