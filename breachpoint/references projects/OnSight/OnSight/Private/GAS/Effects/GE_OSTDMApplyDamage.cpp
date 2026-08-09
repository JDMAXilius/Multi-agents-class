#include "GAS/Effects/GE_OSTDMApplyDamage.h"

#include "GameplayTagContainer.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "TDM/OSTDMDamageExecCalc.h"

UGE_OSTDMApplyDamage::UGE_OSTDMApplyDamage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	{
		const FGameplayTag HitReactionTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.HitReaction"), false);
		if (HitReactionTag.IsValid())
		{
			auto* AssetTags = ObjectInitializer.CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(
				this, TEXT("AssetTags"));
			FInheritedTagContainer TagChanges;
			TagChanges.Added.AddTag(HitReactionTag);
			AssetTags->SetAndApplyAssetTagChanges(TagChanges);
			GEComponents.Add(AssetTags);
		}
	}

	{
		FGameplayModifierInfo DummyMod;
		DummyMod.Attribute = UOSAttributeSet::GetDamageAttribute();
		DummyMod.ModifierOp = EGameplayModOp::Additive;
		DummyMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.f));
		Modifiers.Add(DummyMod);
	}

	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UOSTDMDamageExecCalc::StaticClass();
	Executions.Add(ExecDef);

	{
		const FGameplayTag HitCue = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Hit"), false);
		if (HitCue.IsValid())
		{
			FGameplayEffectCue Cue;
			Cue.GameplayCueTags.AddTag(HitCue);
			GameplayCues.Add(Cue);
		}
	}
}
