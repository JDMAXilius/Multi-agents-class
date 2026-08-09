// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Abilities/OSGameplayAbilityChooser.h"
#include "GameplayAbilities/Public/Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

void UOSGameplayAbilityChooser::PrepTaskFromMontage(UAnimMontage* montage, UGameplayAbility* owningAbility,
	FName taskInstanceName, float rate, FName startSection, bool stopWhenAbilityEnds)
{
	if (!owningAbility) owningAbility = this;
	if (!montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("OSGameplayAbilityChooser: PrepTaskFromMontage called with null montage (%s)"),
			*GetNameSafe(owningAbility));
		return;
	}
	
	auto* task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
	owningAbility,
	taskInstanceName,
	montage,
	rate,
	startSection,
	stopWhenAbilityEnds,
	1.f
	);
	
	if (!task)
	{
		UE_LOG(LogTemp, Warning, TEXT("OSGameplayAbilityChooser: failed to create montage task (%s)"),
			*GetNameSafe(owningAbility));
		return;
	}
	ConnectTask(task);
	task->ReadyForActivation();
}

void UOSGameplayAbilityChooser::ConnectTask(UAbilityTask* task)
{
}
