// Copyright Zollpa LLC.


#include "DamageExecutionCalculation.h"
#include "ZoransResistance/AbilitySystem/Attributes/HealthAttributes.h"
#include "ZoransResistance/AbilitySystem/Data/ZoransGameplayTags.h"

struct FCapturedAttributes
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);

	FCapturedAttributes()
	{
		DEFINE_ATTRIBUTE_CAPTUREDEF(UHealthAttributes, Damage, Source, true);
	}
};

static const FCapturedAttributes& CapturedAttributes()
{
	static FCapturedAttributes StaticCapturedAttributes;
	return StaticCapturedAttributes;
}

UDamageExecutionCalculation::UDamageExecutionCalculation()
{
	RelevantAttributesToCapture.Add(CapturedAttributes().DamageDef);
}

void UDamageExecutionCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const float Damage = Spec.GetSetByCallerMagnitude(GameplayTag_Damage, true);
}