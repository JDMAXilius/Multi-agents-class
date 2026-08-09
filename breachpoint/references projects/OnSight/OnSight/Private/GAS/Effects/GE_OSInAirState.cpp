// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSInAirState.h"

#include "Data/OSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSInAirState::UGE_OSInAirState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	// Infinite: stays active until explicitly removed (same pattern as GE_OSSprintState, GE_OSDeathState).
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Grant Gameplay.State.InAir to the target via UTargetTagsGameplayEffectComponent.
	// Tag-only effect — no attribute modifiers, no GameplayCues.
	//
	// Why UTargetTagsGameplayEffectComponent instead of InheritableOwnedTagsContainer:
	//   UE 5.3+ deprecated fields are dead at runtime for pure C++ GEs. CachedGrantedTags
	//   (the only value read at runtime) is populated exclusively by GEComponents.
	//   PostCDOCompiled (the auto-upgrade path) only runs for Blueprint GEs.
	//   See GE_OSSprintState.cpp and GE_OSDeathState.cpp for the same pattern.
	{
		auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("TargetTags"));

		FInheritedTagContainer TagChanges;
		TagChanges.Added.AddTag(Tags.IsInAir);
		TagChanges.Added.AddTag(Tags.State_Movement_Airborne);
		TargetTags->SetAndApplyTargetTagChanges(TagChanges);

		GEComponents.Add(TargetTags);
	}
}
