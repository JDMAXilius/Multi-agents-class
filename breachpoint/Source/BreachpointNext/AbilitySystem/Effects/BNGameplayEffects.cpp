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
	{
		// 900/600 in the founder's working reference (MyCharacter.h:297,303), expressed as the ratio.
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetSprintSpeedMultiplierAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(1.5f);
		Modifiers.Add(Modifier);
	}
}

UBNGE_State::UBNGE_State()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

UBNGE_Sprint::UBNGE_Sprint()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FAttributeBasedFloat FromMultiplier;
	FromMultiplier.Coefficient = FScalableFloat(1.f);
	FromMultiplier.AttributeCalculationType = EAttributeBasedFloatCalculationType::AttributeMagnitude;
	// Non-snapshot: a later change to the multiplier re-evaluates a sprint already running.
	FromMultiplier.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(
		UBNAttributeSet::GetSprintSpeedMultiplierAttribute(),
		EGameplayEffectAttributeCaptureSource::Target,
		/*bSnapshot=*/false);

	FGameplayModifierInfo Modifier;
	Modifier.ModifierOp = EGameplayModOp::MultiplyAdditive;
	Modifier.Attribute = UBNAttributeSet::GetMoveSpeedAttribute();
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FromMultiplier);
	Modifiers.Add(Modifier);
}
