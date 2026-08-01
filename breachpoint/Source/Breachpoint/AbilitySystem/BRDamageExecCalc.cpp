// Breachpoint. THE damage rule. Base magnitude x flat Damage.* multipliers, from data.

#include "AbilitySystem/BRDamageExecCalc.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

#include "AbilitySystem/BRAttributeSet.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"

namespace BRDamageExecInternal
{
	/**
	 * The `Damage` tag-tree node, derived from a known leaf rather than looked up by string.
	 *
	 * `RequestGameplayTag(FName("Damage"))` would work and would also introduce a spelling of the
	 * family that no compiler checks and that returns an INVALID tag if §3.1 ever renames the
	 * family — which would silently switch every multiplier off. Asking a native leaf for its
	 * parent cannot go stale: if `Damage.Kinetic` compiles, this is `Damage`.
	 */
	static FGameplayTag DamageFamilyRoot()
	{
		static const FGameplayTag Root = BRGameplayTags::Damage_Kinetic.GetTag().RequestDirectParent();
		return Root;
	}
}

UBRDamageExecCalc::UBRDamageExecCalc()
{
	// No RelevantAttributesToCapture. See the header: this game has no mitigation attribute, and a
	// speculative capture costs every damage event an aggregator snapshot nobody reads.
}

FName UBRDamageExecCalc::MakeMultiplierCurveName(const FGameplayTag& DamageTag)
{
	return FName(*(DamageTag.ToString() + BRCombatCurves::Names::DamageMultiplierSuffix));
}

float UBRDamageExecCalc::ComputeFinalDamage(float BaseDamage, const FGameplayTagContainer& DamageTags, TFunctionRef<bool(FName, float&)> CurveLookup, TArray<FName>* OutMissingCurves)
{
	if (BaseDamage < 0.f)
	{
		// REFUSED at the front door. Negative damage is not healing (healing is a Health modifier
		// with its own effect and its own tags); allowing it here would make every multiplier below
		// a potential heal, and a negative multiplier in a CSV a potential exploit.
		return 0.f;
	}

	float Multiplier = 1.f;
	const FGameplayTag DamageRoot = BRDamageExecInternal::DamageFamilyRoot();

	for (const FGameplayTag& Tag : DamageTags)
	{
		// Only the Damage.* family participates. The spec also carries whatever else the caller
		// stamped on it; those are not coefficients.
		if (!Tag.IsValid() || !Tag.MatchesTag(DamageRoot))
		{
			continue;
		}

		const FName CurveName = MakeMultiplierCurveName(Tag);

		float TagMultiplier = 0.f;
		if (CurveLookup(CurveName, TagMultiplier))
		{
			Multiplier *= TagMultiplier;
		}
		else if (OutMissingCurves)
		{
			// The identity is applied (see the header) and the gap is REPORTED rather than
			// swallowed — the caller decides how loud to be about it.
			OutMissingCurves->AddUnique(CurveName);
		}
	}

	// A negative product is reachable only from a negative number in the CSV. Refuse it here rather
	// than let it reach the attribute set, where it would be logged as an impossible input.
	return FMath::Max(BaseDamage * Multiplier, 0.f);
}

void UBRDamageExecCalc::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// WarnIfNotFound=false, default 0: a spec that reached this execution without a BaseDamage is a
	// caller bug, and we say so on our own channel with the effect named, rather than let GAS log
	// an anonymous warning about a missing SetByCaller.
	const float BaseDamage = Spec.GetSetByCallerMagnitude(BRGameplayTags::SetByCaller_BaseDamage, /*WarnIfNotFound=*/false, /*DefaultIfNotFound=*/0.f);

	FGameplayTagContainer AllTags;
	Spec.GetAllAssetTags(AllTags);

	TArray<FName> MissingCurves;
	const float FinalDamage = ComputeFinalDamage(BaseDamage, AllTags,
		[](FName CurveName, float& OutValue) { return BRCombatCurves::Evaluate(CurveName, OutValue); },
		&MissingCurves);

	if (BaseDamage <= 0.f)
	{
		// Zero is legal (a fully mitigated hit is still a hit that did nothing) but a spec with NO
		// BaseCurve at all is not, and the two are indistinguishable downstream. Say it here.
		UE_LOG(LogBRCombat, Warning, TEXT("BRDamageExecCalc: effect '%s' executed with BaseDamage %.3f. If that was not intended, the applier did not set SetByCaller.BaseDamage."),
			*GetNameSafe(Spec.Def), BaseDamage);
	}

	for (const FName& MissingCurve : MissingCurves)
	{
		UE_LOG(LogBRCombat, Warning, TEXT("BRDamageExecCalc: CT_Combat has no curve '%s'; that modifier contributed the identity 1.0. Add the row or remove the tag."),
			*MissingCurve.ToString());
	}

	UE_LOG(LogBRCombat, Verbose, TEXT("BRDamageExecCalc: base %.3f -> final %.3f (tags: %s)."), BaseDamage, FinalDamage, *AllTags.ToStringSimple());

	// The one output: the meta attribute. UBRAttributeSet::PostGameplayEffectExecute consumes it,
	// splits shields/health, gates regen and detects death. Nothing else in the project may write
	// Health or Shields as a consequence of a hit.
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UBRAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, FinalDamage));
}
