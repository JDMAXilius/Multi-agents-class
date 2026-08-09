#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ExecCalc_OSDamage.generated.h"

/**
 * Damage execution:
 * - Reads incoming damage from SetByCaller Data.Damage.
 * - If target is blocking, drains stamina up to incoming damage (absorb) and applies only overflow as Damage meta.
 * - If guard break occurs (incoming > current stamina while blocking), sets GuardBreak meta to 1 (AttributeSet fires event).
 */
UCLASS()
class ONSIGHT_API UExecCalc_OSDamage : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UExecCalc_OSDamage();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};

