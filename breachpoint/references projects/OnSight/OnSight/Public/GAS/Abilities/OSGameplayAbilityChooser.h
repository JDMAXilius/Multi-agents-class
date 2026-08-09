// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Base for abilities that use a Chooser table to pick behavior (e.g. dodge direction, hit reaction type). Resolves Chooser result to ability or params.

#include "CoreMinimal.h"
#include "OSGameplayAbility.h"
#include "OSGameplayAbilityChooser.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSGameplayAbilityChooser : public UOSGameplayAbility
{
	GENERATED_BODY()
protected:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UChooserTable> ChooserTable;
	
	// Prepares Task for activation. Has a default functionality. Override it if you are using something else.
	virtual void PrepTaskFromMontage(UAnimMontage* montage, 
		UGameplayAbility* owningAbility = nullptr,
		FName taskInstanceName = "Montage", 
		float rate = 1.0f, 
		FName startSection = NAME_None, 
		bool stopWhenAbilityEnds = true);
	
	// Connects Task's events to functions.
	virtual void ConnectTask(UAbilityTask* task);
};
