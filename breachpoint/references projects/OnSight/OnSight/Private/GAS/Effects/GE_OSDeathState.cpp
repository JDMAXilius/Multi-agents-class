// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSDeathState.h"

#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"

UGE_OSDeathState::UGE_OSDeathState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// IMPORTANT: Add a harmless "always-valid" modifier so GameplayCues can't be blocked by
	// "Require Modifier Success to Trigger Cues" (some projects enable this on effects).
	// This has no gameplay impact (0 magnitude) and simply guarantees the effect has a valid modifier.
	{
		FGameplayModifierInfo DummyMod;
		DummyMod.Attribute = UOSAttributeSet::GetDamageAttribute();
		DummyMod.ModifierOp = EGameplayModOp::Additive;
		DummyMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.f));
		Modifiers.Add(DummyMod);
	}

	// Grant IsDead tag to target via UTargetTagsGameplayEffectComponent.
	// MIGRATION NOTE: The old InheritableOwnedTagsContainer field is dead at runtime for pure C++ GEs —
	// GetGrantedTags() returns CachedGrantedTags which is only populated by GEComponents, and
	// PostCDOCompiled (the auto-upgrade path) only runs for Blueprints.
	// SetAndApplyTargetTagChanges writes directly to Owner->CachedGrantedTags during construction.
	// CreateDefaultSubobject is required — FindOrAddComponent uses NewObject, forbidden in CDO constructors.
	{
		// Rework Solution
		auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("TargetTags"));

		FInheritedTagContainer TagChanges;
		TagChanges.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsDead"), true));
		TargetTags->SetAndApplyTargetTagChanges(TagChanges);

		GEComponents.Add(TargetTags);
	}

	// Optional cue (can be bound to Wwise+Niagara via GameplayCue manager)
	const FGameplayTag DeathCue = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.VFX.Death"), true);
	if (DeathCue.IsValid())
	{
		FGameplayEffectCue Cue;
		Cue.GameplayCueTags.AddTag(DeathCue);
		GameplayCues.Add(Cue);
	}
}

