// Breachpoint. THE generic GameplayEffect library — seven classes, no assets, no proliferation.

#include "AbilitySystem/Effects/BRGameplayEffects.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

#include "AbilitySystem/BRAttributeSet.h"
#include "AbilitySystem/BRDamageExecCalc.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"

namespace BRGameplayEffectsInternal
{
	/**
	 * THE PATTERN, factored into one function so all seven classes provably do the same thing.
	 *
	 * Creates the granted-tags component, registers it in GEComponents, and — the step that is
	 * easy to miss — pushes the tags through SetAndApplyTargetTagChanges, which is what writes
	 * the effect's CachedGrantedTags. Setting InheritableGrantedTagsContainer without that call
	 * yields an effect that debug-prints correctly and grants nothing at runtime.
	 */
	static void GrantTagToTarget(TArray<TObjectPtr<UGameplayEffectComponent>>& Components, UTargetTagsGameplayEffectComponent* TagsComponent, const FGameplayTag& TagToGrant)
	{
		check(TagsComponent);
		Components.Add(TagsComponent);

		FInheritedTagContainer GrantedTags;
		GrantedTags.AddTag(TagToGrant);
		TagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	}

	/**
	 * FSetByCallerFloat has a user-provided default constructor, so it is not an aggregate and
	 * cannot be brace-initialised. One helper beats four two-line blocks that each look like they
	 * might differ.
	 */
	static FSetByCallerFloat MakeSetByCaller(const FName& DataName, const FGameplayTag& DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataName = DataName;
		SetByCaller.DataTag = DataTag;
		return SetByCaller;
	}
}

// ===========================================================================================
// GE_RecentDamage
// ===========================================================================================

const FName UBRGE_RecentDamage::DurationSetByCallerName(TEXT("RecentDamageDuration"));

UBRGE_RecentDamage::UBRGE_RecentDamage()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// The duration is a gameplay number, so it is NOT here. UBRAbilitySystemComponent reads
	// Shields.Regen.DelaySeconds from CT_Combat and writes it onto the spec.
	DurationMagnitude = FGameplayEffectModifierMagnitude(BRGameplayEffectsInternal::MakeSetByCaller(DurationSetByCallerName, FGameplayTag()));

	// One gate per target, refreshed by every hit. See the header for why this is a rule and not
	// an optimization.
	//
	// THE ONE PLACE R18's CONSTRUCTOR-AUTHORING HITS REAL FRICTION, and it is named rather than
	// hidden: `StackingType` carries UE_DEPRECATED(5.7) ("will be made private, please use
	// GetStackingType"), and the matching `SetStackingType` is **WITH_EDITOR only**. There is
	// therefore NO non-deprecated way for a native GE class to declare its stacking in a packaged
	// build — the API assumes effects are editor-authored assets. Writing the property is legal
	// today and produces C4996; the suppression is scoped to these three lines so that when Epic
	// does privatise it, this fails to compile HERE, loudly, instead of silently losing the gate's
	// stacking. Filed as a contract_gap against R18.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	BRGameplayEffectsInternal::GrantTagToTarget(GEComponents,
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantRecentDamageTag")),
		BRGameplayTags::State_Combat_RecentDamage);
}

// ===========================================================================================
// GE_Damage
// ===========================================================================================

UBRGE_Damage::UBRGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// No Modifiers. The execution owns the arithmetic and writes the result to IncomingDamage.
	FGameplayEffectExecutionDefinition ExecutionDef;
	ExecutionDef.CalculationClass = UBRDamageExecCalc::StaticClass();
	Executions.Add(ExecutionDef);
}

FGameplayEffectSpecHandle UBRGE_Damage::MakeSpec(UAbilitySystemComponent* SourceASC, float BaseDamage, const FGameplayTagContainer& DamageTags, const FGameplayEffectContextHandle& Context)
{
	if (!SourceASC)
	{
		UE_LOG(LogBRCombat, Error, TEXT("UBRGE_Damage::MakeSpec called with a null source ASC. No spec was built; nothing was substituted."));
		return FGameplayEffectSpecHandle();
	}

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(StaticClass(), /*Level=*/1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogBRCombat, Error, TEXT("UBRGE_Damage::MakeSpec: MakeOutgoingSpec failed on '%s'."), *GetNameSafe(SourceASC->GetOwner()));
		return SpecHandle;
	}

	// The magnitude the exec calc starts from. Negative is refused here rather than at the
	// attribute set, where it would already have travelled through a whole pipeline.
	if (BaseDamage < 0.f)
	{
		UE_LOG(LogBRCombat, Error, TEXT("UBRGE_Damage::MakeSpec: negative BaseDamage (%.3f) from '%s' — REFUSED, clamped to zero. Healing is a Health modifier, not negative damage."),
			BaseDamage, *GetNameSafe(SourceASC->GetOwner()));
		BaseDamage = 0.f;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(BRGameplayTags::SetByCaller_BaseDamage, BaseDamage);

	// Dynamic ASSET tags on the SPEC, not on the effect: the effect class stays generic and every
	// hit describes itself. This is what the exec calc reads its multipliers from.
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
		UE_LOG(LogBRCombat, Error, TEXT("UBRGE_Damage::ApplyToTarget: invalid spec handle; nothing applied."));
		return false;
	}

	if (!SourceASC || !TargetASC)
	{
		UE_LOG(LogBRCombat, Error, TEXT("UBRGE_Damage::ApplyToTarget: null %s ASC; nothing applied."), SourceASC ? TEXT("target") : TEXT("source"));
		return false;
	}

	if (!SourceASC->GetOwner() || !SourceASC->GetOwner()->HasAuthority())
	{
		// REFUSED, not "predicted". Damage is server truth: a client-side application would
		// produce a local hit the server never agreed to, and the correction would look like a
		// missed shot rather than a bug.
		UE_LOG(LogBRCombat, Error, TEXT("UBRGE_Damage::ApplyToTarget called without authority on '%s' — REFUSED. Damage is applied on the server only."),
			*GetNameSafe(SourceASC->GetOwner()));
		return false;
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return true;
}

// ===========================================================================================
// GE_Regen
// ===========================================================================================

UBRGE_Regen::UBRGE_Regen()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Period is left at zero HERE and written onto the spec by the applier, from CT_Combat.
	// FGameplayEffectSpec::GetPeriod() returns the SPEC's value for anything non-Instant, so the
	// tick rate is data even though the class is code.
	bExecutePeriodicEffectOnApplication = false;
	PeriodicInhibitionPolicy = EGameplayEffectPeriodInhibitionRemovedPolicy::ResetPeriod;

	FGameplayModifierInfo ShieldsMod;
	ShieldsMod.Attribute = UBRAttributeSet::GetShieldsAttribute();
	ShieldsMod.ModifierOp = EGameplayModOp::AddBase;
	ShieldsMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(BRGameplayEffectsInternal::MakeSetByCaller(NAME_None, BRGameplayTags::SetByCaller_RegenRate));
	Modifiers.Add(ShieldsMod);

	// The gate. Ongoing (not application) requirements: the effect stays applied and goes dormant,
	// so there is no re-application to schedule when the tag expires.
	UTargetTagRequirementsGameplayEffectComponent* Requirements = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("RegenBlockers"));
	Requirements->OngoingTagRequirements.IgnoreTags.AddTag(BRGameplayTags::State_Combat_RecentDamage);
	Requirements->OngoingTagRequirements.IgnoreTags.AddTag(BRGameplayTags::State_Dead);
	GEComponents.Add(Requirements);
}

// ===========================================================================================
// GE_Cooldown
// ===========================================================================================

UBRGE_Cooldown::UBRGE_Cooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(BRGameplayEffectsInternal::MakeSetByCaller(NAME_None, BRGameplayTags::SetByCaller_CooldownDuration));

	// Deliberately no granted-tags component: the tag is per-ability and arrives on the SPEC.
	// See UBRGameplayAbility::ApplyCooldown.
}

// ===========================================================================================
// GE_InitStats
// ===========================================================================================

const FName UBRGE_InitStats::MaxHealthName(TEXT("MaxHealth"));
const FName UBRGE_InitStats::MaxShieldsName(TEXT("MaxShields"));
const FName UBRGE_InitStats::HealthName(TEXT("Health"));
const FName UBRGE_InitStats::ShieldsName(TEXT("Shields"));

UBRGE_InitStats::UBRGE_InitStats()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// ORDER IS LOAD-BEARING. Capacities first, current values second. See the header.
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
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetHealthAttribute(), HealthName));
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetShieldsAttribute(), ShieldsName));
}

// ===========================================================================================
// GE_Death
// ===========================================================================================

UBRGE_Death::UBRGE_Death()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	BRGameplayEffectsInternal::GrantTagToTarget(GEComponents,
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantDeadTag")),
		BRGameplayTags::State_Dead);
}

// ===========================================================================================
// GE_ShieldsBroken
// ===========================================================================================

UBRGE_ShieldsBroken::UBRGE_ShieldsBroken()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	BRGameplayEffectsInternal::GrantTagToTarget(GEComponents,
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantShieldsBrokenTag")),
		BRGameplayTags::State_Shields_Broken);
}
