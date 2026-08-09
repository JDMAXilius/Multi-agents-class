// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSShieldCooldown.h"

#include "Data/OSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSShieldCooldown::UGE_OSShieldCooldown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(5.0f);

	// Grant Cooldown.Shield to target via UTargetTagsGameplayEffectComponent.
	// GA_Shield checks for this tag to prevent re-activation during cooldown.
	// MIGRATION NOTE: The old InheritableOwnedTagsContainer field is dead at runtime for pure C++ GEs —
	// GetGrantedTags() returns CachedGrantedTags which is only populated by GEComponents.
	{
		auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("TargetTags"));

		FInheritedTagContainer TagChanges;
		TagChanges.Added.AddTag(FOSGameplayTags::Get().Cooldown_Shield);
		TargetTags->SetAndApplyTargetTagChanges(TagChanges);

		GEComponents.Add(TargetTags);
	}
}

