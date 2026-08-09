#include "GAS/Effects/GE_OSDebugStunLock.h"

#include "Data/OSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSDebugStunLock::UGE_OSDebugStunLock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Grant Gameplay.State.IsStunLock tag to target via UTargetTagsGameplayEffectComponent.
	auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this, TEXT("TargetTags"));

	FInheritedTagContainer TagChanges;
	TagChanges.Added.AddTag(FOSGameplayTags::Get().State_IsStunLock);
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);

	GEComponents.Add(TargetTags);
}
