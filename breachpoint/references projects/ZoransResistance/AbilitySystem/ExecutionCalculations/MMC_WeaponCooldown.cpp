// Copyright Zollpa LLC


#include "ZoransResistance/AbilitySystem/ExecutionCalculations/MMC_WeaponCooldown.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayAbility.h"


float UMMC_WeaponCooldown::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	if (const UZoransGameplayAbility* Ability = Cast<UZoransGameplayAbility>(Spec.GetContext().GetAbilityInstance_NotReplicated()))
	{
		const float CooldownMagnitude = Ability->AbilityCooldown > 0.f ? Ability->AbilityCooldown : Ability->GetWeaponCooldown();
		return FMath::Clamp<float>(CooldownMagnitude, 0.01f, CooldownMagnitude);
	}

	return 0.01f;
}
