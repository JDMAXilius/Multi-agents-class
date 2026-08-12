#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"

UBNGE_InitAttributes::UBNGE_InitAttributes()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	{
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetHealthAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(100.f);
		Modifiers.Add(Modifier);
	}
	{
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetShieldAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(100.f);
		Modifiers.Add(Modifier);
	}
	{
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetMoveSpeedAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(600.f);
		Modifiers.Add(Modifier);
	}
}
