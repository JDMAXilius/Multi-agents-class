// Breachpoint. THE generic GameplayEffect library — eight classes, no assets, no proliferation.

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
const FName UBRGE_InitStats::MaxGrenadesName(TEXT("MaxGrenades"));
const FName UBRGE_InitStats::GrenadesName(TEXT("Grenades"));

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

	// --- CAPACITIES FIRST. Every ceiling, before any value that is clamped against one. ---------
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetMaxHealthAttribute(), MaxHealthName));
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetMaxShieldsAttribute(), MaxShieldsName));
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetMaxGrenadesAttribute(), MaxGrenadesName));

	// --- CURRENT VALUES SECOND. Each one clamps against the ceiling written above. --------------
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetHealthAttribute(), HealthName));
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetShieldsAttribute(), ShieldsName));

	// Grenades goes LAST and MaxGrenades goes in the block above — not because grenades are less
	// important, but because `PreAttributeChange` clamps Grenades to `GetMaxGrenades()` as it
	// currently is. Reversed, MaxGrenades is still 0 when Grenades is written and every fighter
	// spawns with an empty pouch from a table that says 2. The failure is silent, survives respawn,
	// and looks like the grenade ability is broken.
	Modifiers.Add(MakeOverrideMod(UBRAttributeSet::GetGrenadesAttribute(), GrenadesName));
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

// ===========================================================================================
// GE_GrenadeCost
// ===========================================================================================

const FName UBRGE_GrenadeCost::CostSetByCallerName(TEXT("GrenadeCost"));

// The name the attribute MUST have on UBRAttributeSet. Plural noun, no "Carried" suffix, to sit
// beside Shields/Health rather than beside a description of them — and paired with MaxGrenades
// exactly as Shields is paired with MaxShields, because the clamp ceiling has to be an attribute
// for the same reason theirs is: it comes from data, through GE_InitStats, and it replicates.
const FName UBRGE_GrenadeCost::GrenadeCountAttributeName(TEXT("Grenades"));

FGameplayAttribute UBRGE_GrenadeCost::ResolveGrenadeCountAttribute()
{
	// THE BRIDGE. FindFProperty, not FindFieldChecked: the `checked` variant that
	// ATTRIBUTE_ACCESSORS_BASIC generates is correct for an attribute that exists and is a fatal
	// crash for one that does not. We want the refusal.
	FStructProperty* CountProperty = FindFProperty<FStructProperty>(UBRAttributeSet::StaticClass(), GrenadeCountAttributeName);
	if (!CountProperty || !CountProperty->Struct || !CountProperty->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
	{
		// Also the answer when someone declares `int32 Grenades` on the set: a property of the
		// right NAME and the wrong TYPE is not the attribute, and treating it as one would build a
		// modifier that writes through a pointer GAS cannot evaluate.
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
	// Instant, like every cost: the spend must move the attribute's BASE value, so that it
	// survives, replicates, and is the thing GE_InitStats overrides on respawn. A duration-based
	// cost would be a temporary debuff that expires and hands the grenade back.
	DurationPolicy = EGameplayEffectDurationType::Instant;

	const FGameplayAttribute GrenadeCount = ResolveGrenadeCountAttribute();
	if (!GrenadeCount.IsValid())
	{
		// LOUD, ONCE, AT MODULE LOAD — the CDO is constructed exactly once, so this is one line in
		// the log rather than spam, and it is an Error rather than a Warning because the visible
		// symptom on the far side of it is "grenades are free", which reads as a design decision.
		//
		// NO MODIFIER IS ADDED. An effect holding a modifier with an invalid attribute is not a
		// safer version of an empty one; it is an effect that applies successfully and charges
		// nothing, at whatever moment the attribute half of this feature is believed to be done.
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGE_GrenadeCost: UBRAttributeSet has no '%s' FGameplayAttributeData property, so the grenade cost is INERT and grenades remain FREE. "
				 "This effect must not be wired as an ability cost until the attribute lands (see the class comment for the full specification hand-off)."),
			*GrenadeCountAttributeName.ToString());
		return;
	}

	// ONE modifier, and no number: `AddBase` with a SetByCaller magnitude the applier fills in
	// from data. The sign lives in MakeSpec — see the header's trap 1.
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
		UE_LOG(LogBRCombat, Error, TEXT("UBRGE_GrenadeCost::MakeSpec called with a null ASC. No spec was built; nothing was substituted."));
		return FGameplayEffectSpecHandle();
	}

	if (!IsOperational())
	{
		// REFUSED rather than "applied harmlessly". A cost that applies and charges nothing is the
		// exact bug this class was written to close, and it would close the ticket's Done-when box
		// while leaving the grenade free.
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGE_GrenadeCost::MakeSpec on '%s': the '%s' attribute does not exist on UBRAttributeSet, so no cost spec can be built. The throw must be REFUSED or left free deliberately — not charged to nothing."),
			*GetNameSafe(SpenderASC->GetOwner()), *GrenadeCountAttributeName.ToString());
		return FGameplayEffectSpecHandle();
	}

	if (GrenadesPerThrow <= 0.f)
	{
		// Zero is not "free", it is "the data row is missing" — the same position UBRGA_Grenade
		// takes on all six of its tuning numbers, and the same reason: unset and absurd are the
		// same value here, so one check covers both.
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGE_GrenadeCost::MakeSpec on '%s': cost of %.3f grenades is not a cost — REFUSED. The count comes from data and may not be invented here."),
			*GetNameSafe(SpenderASC->GetOwner()), GrenadesPerThrow);
		return FGameplayEffectSpecHandle();
	}

	const FGameplayEffectSpecHandle SpecHandle = SpenderASC->MakeOutgoingSpec(StaticClass(), /*Level=*/1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogBRCombat, Error, TEXT("UBRGE_GrenadeCost::MakeSpec: MakeOutgoingSpec failed on '%s'."), *GetNameSafe(SpenderASC->GetOwner()));
		return SpecHandle;
	}

	// THE ONE PLACE THE SIGN IS DECIDED. Callers pass what the CSV says ("a throw costs 1"); the
	// negation that turns it into a decrement happens here and nowhere else, so no caller can get
	// it backwards and no caller has to remember it. FMath::Abs rather than a bare minus because a
	// caller that already negated should still be charged, not refunded.
	SpecHandle.Data->SetSetByCallerMagnitude(CostSetByCallerName, -FMath::Abs(GrenadesPerThrow));

	return SpecHandle;
}
