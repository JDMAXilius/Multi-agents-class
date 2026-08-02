#include "AbilitySystem/Effects/BRGameplayEffects.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "UObject/UnrealType.h"

#include "AbilitySystem/BRAttributeSet.h"
#include "AbilitySystem/BRDamageExecCalc.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"

namespace BRGameplayEffectsInternal
{
	static void GrantTagToTarget(TArray<TObjectPtr<UGameplayEffectComponent>>& Components, UTargetTagsGameplayEffectComponent* TagsComponent, const FGameplayTag& TagToGrant)
	{
		check(TagsComponent);
		Components.Add(TagsComponent);

		FInheritedTagContainer GrantedTags;
		GrantedTags.AddTag(TagToGrant);
		TagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	}

	static FSetByCallerFloat MakeSetByCaller(const FName& DataName, const FGameplayTag& DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataName = DataName;
		SetByCaller.DataTag = DataTag;
		return SetByCaller;
	}
}

const FName UBRGE_RecentDamage::DurationSetByCallerName(TEXT("RecentDamageDuration"));

UBRGE_RecentDamage::UBRGE_RecentDamage()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	DurationMagnitude = FGameplayEffectModifierMagnitude(BRGameplayEffectsInternal::MakeSetByCaller(DurationSetByCallerName, FGameplayTag()));

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	BRGameplayEffectsInternal::GrantTagToTarget(GEComponents,
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantRecentDamageTag")),
		BRGameplayTags::State_Combat_RecentDamage);
}

UBRGE_Damage::UBRGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition ExecutionDef;
	ExecutionDef.CalculationClass = UBRDamageExecCalc::StaticClass();
	Executions.Add(ExecutionDef);
}

FGameplayEffectSpecHandle UBRGE_Damage::MakeSpec(UAbilitySystemComponent* SourceASC, float BaseDamage, const FGameplayTagContainer& DamageTags, const FGameplayEffectContextHandle& Context)
{
	if (!SourceASC)
	{
		return FGameplayEffectSpecHandle();
	}

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(StaticClass(), 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return SpecHandle;
	}

	if (BaseDamage < 0.f)
	{
		BaseDamage = 0.f;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(BRGameplayTags::SetByCaller_BaseDamage, BaseDamage);

	for (const FGameplayTag& Tag : DamageTags)
	{
		SpecHandle.Data->AddDynamicAssetTag(Tag);
	}

	return SpecHandle;
}

bool UBRGE_Damage::ApplyToTarget(const FGameplayEffectSpecHandle& SpecHandle, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC)
{
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	if (!SourceASC || !TargetASC)
	{
		return false;
	}

	if (!SourceASC->GetOwner() || !SourceASC->GetOwner()->HasAuthority())
	{
		return false;
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return true;
}

UBRGE_Regen::UBRGE_Regen()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	bExecutePeriodicEffectOnApplication = false;
	PeriodicInhibitionPolicy = EGameplayEffectPeriodInhibitionRemovedPolicy::ResetPeriod;

	FGameplayModifierInfo ShieldsMod;
	ShieldsMod.Attribute = UBRAttributeSet::GetShieldsAttribute();
	ShieldsMod.ModifierOp = EGameplayModOp::AddBase;
	ShieldsMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(BRGameplayEffectsInternal::MakeSetByCaller(NAME_None, BRGameplayTags::SetByCaller_RegenRate));
	Modifiers.Add(ShieldsMod);

	UTargetTagRequirementsGameplayEffectComponent* Requirements = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("RegenBlockers"));
	Requirements->OngoingTagRequirements.IgnoreTags.AddTag(BRGameplayTags::State_Combat_RecentDamage);
	Requirements->OngoingTagRequirements.IgnoreTags.AddTag(BRGameplayTags::State_Dead);
	GEComponents.Add(Requirements);
}

UBRGE_Cooldown::UBRGE_Cooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(BRGameplayEffectsInternal::MakeSetByCaller(NAME_None, BRGameplayTags::SetByCaller_CooldownDuration));
}

const FName UBRGE_InitStats::MaxHealthName(TEXT("MaxHealth"));
const FName UBRGE_InitStats::MaxShieldsName(TEXT("MaxShields"));
const FName UBRGE_InitStats::HealthName(TEXT("Health"));
const FName UBRGE_InitStats::ShieldsName(TEXT("Shields"));
const FName UBRGE_InitStats::MaxGrenadesName(TEXT("MaxGrenades"));
const FName UBRGE_InitStats::GrenadesName(TEXT("Grenades"));

UBRGE_InitStats::UBRGE_InitStats()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	auto MakeOverrideMod = [](const FGameplayAttribute& Attribute, const FName& SetByCallerName)
	{
		FGameplayModifierInfo Mod;
		Mod.Attribute = Attribute;
		Mod.ModifierOp = EGameplayModOp::Override;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(BRGameplayEffectsInternal::MakeSetByCaller(SetByCallerName, FGameplayTag()));
		return Mod;
	};

	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetMaxHealthAttribute(), MaxHealthName));
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetMaxShieldsAttribute(), MaxShieldsName));
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetMaxGrenadesAttribute(), MaxGrenadesName));

	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetHealthAttribute(), HealthName));
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetShieldsAttribute(), ShieldsName));

	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetGrenadesAttribute(), GrenadesName));
}

UBRGE_Death::UBRGE_Death()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	BRGameplayEffectsInternal::GrantTagToTarget(GEComponents,
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantDeadTag")),
		BRGameplayTags::State_Dead);
}

UBRGE_ShieldsBroken::UBRGE_ShieldsBroken()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	BRGameplayEffectsInternal::GrantTagToTarget(GEComponents,
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantShieldsBrokenTag")),
		BRGameplayTags::State_Shields_Broken);
}

const FName UBRGE_GrenadeCost::CostSetByCallerName(TEXT("GrenadeCost"));

const FName UBRGE_GrenadeCost::GrenadeCountAttributeName(TEXT("Grenades"));

FGameplayAttribute UBRGE_GrenadeCost::ResolveGrenadeCountAttribute()
{
	FStructProperty* CountProperty = FindFProperty<FStructProperty>(UBRAttributeSet::StaticClass(), GrenadeCountAttributeName);
	if (!CountProperty || !CountProperty->Struct || !CountProperty->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
	{
		return FGameplayAttribute();
	}

	return FGameplayAttribute(CountProperty);
}

bool UBRGE_GrenadeCost::IsOperational()
{
	return ResolveGrenadeCountAttribute().IsValid();
}

UBRGE_GrenadeCost::UBRGE_GrenadeCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	const FGameplayAttribute GrenadeCount = ResolveGrenadeCountAttribute();
	if (!GrenadeCount.IsValid())
	{
		return;
	}

	FGameplayModifierInfo CostMod;
	CostMod.Attribute = GrenadeCount;
	CostMod.ModifierOp = EGameplayModOp::AddBase;
	CostMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(BRGameplayEffectsInternal::MakeSetByCaller(CostSetByCallerName, FGameplayTag()));
	Modifiers.Add(CostMod);
}

FGameplayEffectSpecHandle UBRGE_GrenadeCost::MakeSpec(UAbilitySystemComponent* SpenderASC, float GrenadesPerThrow, const FGameplayEffectContextHandle& Context)
{
	if (!SpenderASC)
	{
		return FGameplayEffectSpecHandle();
	}

	if (!IsOperational())
	{
		return FGameplayEffectSpecHandle();
	}

	if (GrenadesPerThrow <= 0.f)
	{
		return FGameplayEffectSpecHandle();
	}

	const FGameplayEffectSpecHandle SpecHandle = SpenderASC->MakeOutgoingSpec(StaticClass(), 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return SpecHandle;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(CostSetByCallerName, -FMath::Abs(GrenadesPerThrow));

	return SpecHandle;
}
