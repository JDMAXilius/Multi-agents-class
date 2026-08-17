#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"

#include "BRDamageExecCalc.generated.h"

UCLASS(meta = (DisplayName = "BR Damage Execution"))
class BREACHPOINT_API UBRDamageExecCalc : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UBRDamageExecCalc();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

	static float ComputeFinalDamage(float BaseDamage, const FGameplayTagContainer& DamageTags, TFunctionRef<bool(FName, float&)> CurveLookup, TArray<FName>* OutMissingCurves = nullptr);

	static FName MakeMultiplierCurveName(const FGameplayTag& DamageTag);
};
