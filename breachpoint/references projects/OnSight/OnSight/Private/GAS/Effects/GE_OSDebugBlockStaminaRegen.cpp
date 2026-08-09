#include "GAS/Effects/GE_OSDebugBlockStaminaRegen.h"

#include "Data/OSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSDebugBlockStaminaRegen::UGE_OSDebugBlockStaminaRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this, TEXT("TargetTags"));

	FInheritedTagContainer TagChanges;
	TagChanges.Added.AddTag(FOSGameplayTags::Get().Debug_BlockStaminaRegen);
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);

	GEComponents.Add(TargetTags);
}
