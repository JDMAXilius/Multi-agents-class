// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Effects/GE_OSBlockState.h"

#include "Data/OSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"


UGE_OSBlockState::UGE_OSBlockState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	//---------------------------------------------------------------------- 
	//  Duration
	//---------------------------------------------------------------------- 
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	//---------------------------------------------------------------------- 
	//  Tags
	//---------------------------------------------------------------------- 
	{
		auto* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("TargetTags"));

		FInheritedTagContainer TagChanges;

		// Only the effect marker tag is needed here for query/removal fallback
		TagChanges.Added.AddTag(Tags.Effect_Movement_Block);

		TargetTags->SetAndApplyTargetTagChanges(TagChanges);
		GEComponents.Add(TargetTags);
	}
	//---------------------------------------------------------------------- 
	//  Movement Speed Modifier
	//---------------------------------------------------------------------- 
	FGameplayModifierInfo Mod;
	Mod.Attribute = UOSAttributeSet::GetMovementSpeedAttribute();
	Mod.ModifierOp = EGameplayModOp::Override;

	FSetByCallerFloat SBC;
	SBC.DataTag = Tags.Data_Movement_SpeedMultiplier;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SBC);

	Modifiers.Add(Mod);
}