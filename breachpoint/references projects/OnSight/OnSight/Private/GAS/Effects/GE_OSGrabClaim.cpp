#include "GAS/Effects/GE_OSGrabClaim.h"

#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSGrabClaim::UGE_OSGrabClaim(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Grant IsBeingGrabbed tag to the victim via UTargetTagsGameplayEffectComponent.
	auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this, TEXT("TargetTags"));

	FInheritedTagContainer TagChanges;
	TagChanges.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsBeingGrabbed"), true));
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);

	GEComponents.Add(TargetTags);
}
