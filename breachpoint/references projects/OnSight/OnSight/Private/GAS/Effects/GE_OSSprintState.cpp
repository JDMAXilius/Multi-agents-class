// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSSprintState.h"

#include "Data/OSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"

UGE_OSSprintState::UGE_OSSprintState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Grant IsSprinting + Effect_Movement_Sprint tags to target via UTargetTagsGameplayEffectComponent.
	// IsSprinting: read by AnimInstance (CombatState + tag bools), attack abilities (ActivationBlockedTags),
	//   regen GEs (IgnoreTags), and OSAnimNotify_CancelActionBuffer.
	// Effect_Movement_Sprint: used to query/remove this specific effect by tag.
	// MIGRATION NOTE: The old InheritableOwnedTagsContainer field is dead at runtime for pure C++ GEs —
	// GetGrantedTags() returns CachedGrantedTags which is only populated by GEComponents.
	{
		auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("TargetTags"));

		FInheritedTagContainer TagChanges;
		TagChanges.Added.AddTag(Tags.IsSprinting);          // State tag for animation/logic
		TagChanges.Added.AddTag(Tags.Effect_Movement_Sprint); // Query/remove sprint effect by tag
		TargetTags->SetAndApplyTargetTagChanges(TagChanges);

		GEComponents.Add(TargetTags);
	}

	// Override MovementSpeed multiplier from SetByCaller.
	FGameplayModifierInfo Mod;
	Mod.Attribute = UOSAttributeSet::GetMovementSpeedAttribute();
	Mod.ModifierOp = EGameplayModOp::Override;

	FSetByCallerFloat SBC;
	SBC.DataTag = Tags.Data_Movement_SpeedMultiplier;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SBC);

	Modifiers.Add(Mod);
}

