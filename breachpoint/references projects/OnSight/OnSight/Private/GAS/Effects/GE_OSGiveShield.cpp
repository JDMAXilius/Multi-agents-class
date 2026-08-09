// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSGiveShield.h"

#include "Data/OSGameplayTags.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSGiveShield::UGE_OSGiveShield(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(15.0f);

	// Grant Status.Buff.Shield to target via UTargetTagsGameplayEffectComponent.
	// MIGRATION NOTE: The old InheritableOwnedTagsContainer field is dead at runtime for pure C++ GEs —
	// GetGrantedTags() returns CachedGrantedTags which is only populated by GEComponents, and
	// PostCDOCompiled (the auto-upgrade path) only runs for Blueprints.
	// SetAndApplyTargetTagChanges writes directly to Owner->CachedGrantedTags during construction.
	// CreateDefaultSubobject is required — FindOrAddComponent uses NewObject, forbidden in CDO constructors.
	{
		auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("TargetTags"));

		FInheritedTagContainer TagChanges;
		TagChanges.Added.AddTag(FOSGameplayTags::Get().Status_Buff_Shield);
		TargetTags->SetAndApplyTargetTagChanges(TagChanges);

		GEComponents.Add(TargetTags);
	}

	// Gameplay Cue: GameplayCue.ShieldBubble
	const FGameplayTag CueTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.ShieldBubble"), true);
	FGameplayEffectCue Cue;
	Cue.GameplayCueTags.AddTag(CueTag);
	GameplayCues.Add(Cue);
}

