#include "GAS/Effects/GE_OSDebugNoStaminaDrain.h"

#include "Data/OSGameplayTags.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSDebugNoStaminaDrain::UGE_OSDebugNoStaminaDrain(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Grant debug tag for OSHUD status strip display.
	auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this, TEXT("TargetTags"));

	FInheritedTagContainer TagChanges;
	TagChanges.Added.AddTag(FOSGameplayTags::Get().Debug_NoStaminaDrain);
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);

	GEComponents.Add(TargetTags);

	// Override Stamina to a value above MaxStamina. The standard PreAttributeChange
	// clamp (FMath::Clamp 0..MaxStamina) brings it down to MaxStamina. Net effect:
	// stamina always reads MaxStamina while this GE is active — pure GE pipeline,
	// no direct attribute manipulation.
	FGameplayModifierInfo Mod;
	Mod.Attribute = UOSAttributeSet::GetStaminaAttribute();
	Mod.ModifierOp = EGameplayModOp::Override;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(9999.f));
	Modifiers.Add(Mod);
}
