#include "GAS/Effects/GE_OSGroundedState.h"

#include "Data/OSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSGroundedState::UGE_OSGroundedState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	DurationPolicy = EGameplayEffectDurationType::Infinite;

	{
		auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("TargetTags"));

		FInheritedTagContainer TagChanges;
		TagChanges.Added.AddTag(Tags.State_Movement_Grounded);
		TargetTags->SetAndApplyTargetTagChanges(TagChanges);

		GEComponents.Add(TargetTags);
	}
}
