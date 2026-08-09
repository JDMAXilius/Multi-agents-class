// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSHitReactState.h"

#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSHitReactState::UGE_OSHitReactState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(0.35f);

	// Grant IsHitReacting to target via UTargetTagsGameplayEffectComponent.
	// This tag is read by regen GEs (IgnoreTags) and AnimInstance (CombatState + tag bools).
	// MIGRATION NOTE: The old InheritableOwnedTagsContainer field is dead at runtime for pure C++ GEs —
	// GetGrantedTags() returns CachedGrantedTags which is only populated by GEComponents, and
	// PostCDOCompiled (the auto-upgrade path) only runs for Blueprints.
	// SetAndApplyTargetTagChanges writes directly to Owner->CachedGrantedTags during construction.
	{
		auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("TargetTags"));

		FInheritedTagContainer TagChanges;
		TagChanges.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsHitReacting"), true));
		TargetTags->SetAndApplyTargetTagChanges(TagChanges);

		GEComponents.Add(TargetTags);
	}
}

