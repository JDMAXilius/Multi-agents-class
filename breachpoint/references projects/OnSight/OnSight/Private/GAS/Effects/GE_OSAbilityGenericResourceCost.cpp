// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSAbilityGenericResourceCost.h"

#include "Data/OSGameplayTags.h"
#include "GAS/Attributes/OSAttributeSet.h"

UGE_OSAbilityGenericResourceCost::UGE_OSAbilityGenericResourceCost(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	auto AddCostModifier = [this](const FGameplayAttribute& Attr, const FGameplayTag& DataTag)
	{
		if (!Attr.IsValid() || !DataTag.IsValid())
		{
			return;
		}
		FGameplayModifierInfo Mod;
		Mod.Attribute = Attr;
		Mod.ModifierOp = EGameplayModOp::Additive;
		FSetByCallerFloat SBC;
		SBC.DataTag = DataTag;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SBC);
		Modifiers.Add(Mod);
	};

	AddCostModifier(UOSAttributeSet::GetHealthAttribute(), Tags.Data_Cost_Health);
	AddCostModifier(UOSAttributeSet::GetAuraAttribute(), Tags.Data_Cost_Aura);
	AddCostModifier(UOSAttributeSet::GetStaminaAttribute(), Tags.Data_Cost_Stamina);
}
