// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSStaminaRegen.h"

#include "Data/OSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"

UGE_OSStaminaRegen::UGE_OSStaminaRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 1.0f;
	bExecutePeriodicEffectOnApplication = true;

	// --- Ongoing Tag Requirements (UE 5.3+ component-based) ---
	// Inhibit this effect while any of these tags are present on the target.
	// Uses UTargetTagRequirementsGameplayEffectComponent (the runtime path).
	// The deprecated OngoingTagRequirements field is no longer read at runtime (empty function body).
	// CreateDefaultSubobject is required here — FindOrAddComponent uses NewObject which is
	// forbidden inside CDO constructors.
	{
		// Rework Solution
		auto* TagReqs = ObjectInitializer.CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(
			this, TEXT("OngoingTagRequirements"));

		// Stop ticking while dead or in active combat.
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsDead);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsAttacking);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsCharging);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsBlocking);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsHitReacting);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsDodging);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.Debug_BlockStaminaRegen);

		GEComponents.Add(TagReqs);
	}

	// --- Granted Tag (UE 5.3+ component-based) ---
	// Helpful "owned tag" for querying/cancelling regen effects.
	{
		// Rework Solution
		auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("TargetTags"));

		FInheritedTagContainer TagChanges;
		TagChanges.Added.AddTag(Tags.Effect_Regen_Stamina);
		TargetTags->SetAndApplyTargetTagChanges(TagChanges);

		GEComponents.Add(TargetTags);
	}

	// +Stamina per period (tune via BP child).
	FGameplayModifierInfo Mod;
	Mod.Attribute = UOSAttributeSet::GetStaminaAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.f));
	Modifiers.Add(Mod);
}

