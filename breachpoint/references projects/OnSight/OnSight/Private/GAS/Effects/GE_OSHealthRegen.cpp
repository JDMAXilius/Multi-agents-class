// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSHealthRegen.h"

#include "Data/OSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"

UGE_OSHealthRegen::UGE_OSHealthRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 1.0f;
	bExecutePeriodicEffectOnApplication = true;

	// --- Ongoing Tag Requirements (UE 5.3+ component-based) ---
	// Inhibit this effect while any of these tags are present on the target.
	// The deprecated OngoingTagRequirements field is no longer read at runtime (empty function body).
	// CreateDefaultSubobject is required — FindOrAddComponent uses NewObject, forbidden in CDO constructors.
	{
		auto* TagReqs = ObjectInitializer.CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(
			this, TEXT("OngoingTagRequirements"));

		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsDead);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsAttacking);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsCharging);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsBlocking);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsHitReacting);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.IsDodging);
		TagReqs->OngoingTagRequirements.IgnoreTags.AddTag(Tags.Debug_BlockHealthRegen);

		GEComponents.Add(TagReqs);
	}

	// --- Granted Tag (UE 5.3+ component-based) ---
	{
		auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("TargetTags"));

		FInheritedTagContainer TagChanges;
		TagChanges.Added.AddTag(Tags.Effect_Regen_Health);
		TargetTags->SetAndApplyTargetTagChanges(TagChanges);

		GEComponents.Add(TargetTags);
	}

	// +Health per period (tune via BP child).
	FGameplayModifierInfo Mod;
	Mod.Attribute = UOSAttributeSet::GetHealthAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.f));
	Modifiers.Add(Mod);
}

