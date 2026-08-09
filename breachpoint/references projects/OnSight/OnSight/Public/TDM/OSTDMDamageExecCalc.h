#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "OSTDMDamageExecCalc.generated.h"

/**
 * TDM damage execution (simplified vs UExecCalc_OSDamage: no block/stamina).
 * SetByCaller Data.Damage -> Damage meta -> UOSAttributeSet::PostGameplayEffectExecute (Health, death context, events).
 * Friendly fire blocked via IGenericTeamAgentInterface.
 */
UCLASS()
class ONSIGHT_API UOSTDMDamageExecCalc : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UOSTDMDamageExecCalc();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	                                    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
