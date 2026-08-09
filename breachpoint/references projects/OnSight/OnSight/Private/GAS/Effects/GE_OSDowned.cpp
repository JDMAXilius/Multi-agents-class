// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Effects/GE_OSDowned.h"

#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_OSDowned::UGE_OSDowned(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Grant IsDowned + Invincibility via UTargetTagsGameplayEffectComponent.
	// The legacy InheritableOwnedTagsContainer field is dead at runtime for pure C++ GEs —
	// GetGrantedTags() returns CachedGrantedTags which is only populated by GEComponents.
	auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this, TEXT("TargetTags"));

	FInheritedTagContainer TagChanges;
	TagChanges.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsDowned"), true));
	TagChanges.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Invincibility"), true));
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);

	GEComponents.Add(TargetTags);
}
