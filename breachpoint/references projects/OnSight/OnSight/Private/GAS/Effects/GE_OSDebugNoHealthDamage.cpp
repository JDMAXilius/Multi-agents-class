#include "GAS/Effects/GE_OSDebugNoHealthDamage.h"

#include "Data/OSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSDebugNoHealthDamage::UGE_OSDebugNoHealthDamage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Grant Gameplay.Debug.NoHealthDamage tag to target via UTargetTagsGameplayEffectComponent.
	auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this, TEXT("TargetTags"));

	FInheritedTagContainer TagChanges;
	TagChanges.Added.AddTag(FOSGameplayTags::Get().Debug_NoHealthDamage);
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);

	GEComponents.Add(TargetTags);
}
