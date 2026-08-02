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
	static FGameplayTag DamageFamilyRoot()
	{
		static const FGameplayTag Root = BRGameplayTags::Damage_Kinetic.GetTag().RequestDirectParent();
		return Root;
	}
}

UBRDamageExecCalc::UBRDamageExecCalc()
{
}

FName UBRDamageExecCalc::MakeMultiplierCurveName(const FGameplayTag& DamageTag)
{
	return FName(*(DamageTag.ToString() + BRCombatCurves::Names::DamageMultiplierSuffix));
}

float UBRDamageExecCalc::ComputeFinalDamage(float BaseDamage, const FGameplayTagContainer& DamageTags, TFunctionRef<bool(FName, float&)> CurveLookup, TArray<FName>* OutMissingCurves)
{
	if (BaseDamage < 0.f)
	{
		return 0.f;
	}

	float Multiplier = 1.f;
	const FGameplayTag DamageRoot = BRDamageExecInternal::DamageFamilyRoot();

	for (const FGameplayTag& Tag : DamageTags)
	{
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
			OutMissingCurves->AddUnique(CurveName);
		}
	}

	return FMath::Max(BaseDamage * Multiplier, 0.f);
}

void UBRDamageExecCalc::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const float BaseDamage = Spec.GetSetByCallerMagnitude(BRGameplayTags::SetByCaller_BaseDamage, false, 0.f);

	FGameplayTagContainer AllTags;
	Spec.GetAllAssetTags(AllTags);

	TArray<FName> MissingCurves;
	const float FinalDamage = ComputeFinalDamage(BaseDamage, AllTags,
		[](FName CurveName, float& OutValue) { return BRCombatCurves::Evaluate(CurveName, OutValue); },
		&MissingCurves);

	if (BaseDamage <= 0.f)
	{
		UE_LOG(LogBRCombat, Warning, TEXT("BRDamageExecCalc: effect '%s' executed with BaseDamage %.3f. If that was not intended, the applier did not set SetByCaller.BaseDamage."),
			*GetNameSafe(Spec.Def), BaseDamage);
	}

	for (const FName& MissingCurve : MissingCurves)
	{
		UE_LOG(LogBRCombat, Warning, TEXT("BRDamageExecCalc: CT_Combat has no curve '%s'; that modifier contributed the identity 1.0. Add the row or remove the tag."),
			*MissingCurve.ToString());
	}

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UBRAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, FinalDamage));
}
