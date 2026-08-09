// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Effects/GE_OSApplyDamage.h"

#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "GAS/Executions/ExecCalc_OSDamage.h"

UGE_OSApplyDamage::UGE_OSApplyDamage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Asset tag used by AttributeSet to gate HitReact events (GAS-pure, data-driven).
	// Only damage effects with this tag will trigger GameplayEvent.HitReact.
	// PostGameplayEffectExecute checks Spec.GetAllAssetTags() for this tag before firing HitReact events.
	{
		const FGameplayTag HitReactionTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.HitReaction"), false);
		if (HitReactionTag.IsValid())
		{
			//Rework Solution 
			auto* AssetTags = ObjectInitializer.CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(
				this, TEXT("AssetTags"));

			FInheritedTagContainer TagChanges;
			TagChanges.Added.AddTag(HitReactionTag);
			AssetTags->SetAndApplyAssetTagChanges(TagChanges);

			GEComponents.Add(AssetTags);
		}
	}

	// IMPORTANT: Add a harmless "always-valid" modifier so GameplayCues can't be blocked by
	// "Require Modifier Success to Trigger Cues" (some projects enable this on effects).
	// We use Damage meta-attribute with 0 magnitude; real damage is produced by the ExecCalc.
	{
		FGameplayModifierInfo DummyMod;
		DummyMod.Attribute = UOSAttributeSet::GetDamageAttribute();
		DummyMod.ModifierOp = EGameplayModOp::Additive;
		DummyMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.f));
		Modifiers.Add(DummyMod);
	}

	// Execute damage calculation (block absorb + overflow + guardbreak) in a single authoritative step.
	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UExecCalc_OSDamage::StaticClass();
	Executions.Add(ExecDef);

	
	 /*
	// Authoritative impact sound/vfx cue for ANY damage application.
	// We reuse the existing tag: GameplayCue.Sound.Hit
	{
		// NOTE: Non-fatal lookup to avoid editor hot-reload crashes if tags haven't refreshed yet.
		const FGameplayTag HitCue = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Hit"), false);
		if (HitCue.IsValid())
		{
			FGameplayEffectCue Cue;
			Cue.GameplayCueTags.AddTag(HitCue);
			GameplayCues.Add(Cue);
		}
	}
	*/
}
