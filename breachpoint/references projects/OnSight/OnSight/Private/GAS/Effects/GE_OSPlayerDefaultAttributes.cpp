// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSPlayerDefaultAttributes.h"

#include "GAS/Attributes/OSAttributeSet.h"

UGE_OSPlayerDefaultAttributes::UGE_OSPlayerDefaultAttributes()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	auto AddOverride = [this](const FGameplayAttribute& Attr, float Value)
	{
		FGameplayModifierInfo Mod;
		Mod.Attribute = Attr;
		Mod.ModifierOp = EGameplayModOp::Override;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		Modifiers.Add(Mod);
	};

	// Baselines (tune via BP child if desired).
	constexpr float BaseMaxHealth = 100.f;
	constexpr float BaseMaxStamina = 100.f;
	constexpr float BaseMaxAura = 100.f;
	constexpr float BaseMoveSpeedBaseUnits = 500.f;
	// IMPORTANT: MovementSpeed is a *multiplier* in this project (see `UOSCharacterMovementComponent`).
	// 1.0 = normal walk speed, >1.0 faster, <1.0 slower.
	constexpr float BaseMoveSpeedMultiplier = 1.0f;

	AddOverride(UOSAttributeSet::GetMaxHealthAttribute(), BaseMaxHealth);
	AddOverride(UOSAttributeSet::GetHealthAttribute(), BaseMaxHealth);
	AddOverride(UOSAttributeSet::GetMaxStaminaAttribute(), BaseMaxStamina);
	AddOverride(UOSAttributeSet::GetStaminaAttribute(), BaseMaxStamina);
	AddOverride(UOSAttributeSet::GetMaxAuraAttribute(), BaseMaxAura);
	AddOverride(UOSAttributeSet::GetAuraAttribute(), BaseMaxAura);
	AddOverride(UOSAttributeSet::GetMoveSpeedBaseAttribute(), BaseMoveSpeedBaseUnits);
	AddOverride(UOSAttributeSet::GetMovementSpeedAttribute(), BaseMoveSpeedMultiplier);
}

