#include "GAS/Effects/GE_OSTracking.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSTracking::UGE_OSTracking(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	StackingType = EGameplayEffectStackingType::None;

	const FGameplayTag TrackingTag = FGameplayTag::RequestGameplayTag(TEXT("Character.State.Tracking"), false);
	if (!TrackingTag.IsValid())
	{
		return;
	}

	auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this, TEXT("TargetTags"));

	FInheritedTagContainer TagChanges;
	TagChanges.Added.AddTag(TrackingTag);
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);
	GEComponents.Add(TargetTags);
}
