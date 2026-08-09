// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/GA_Shield.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Data/OSGameplayTags.h"
#include "GAS/Effects/GE_OSGiveShield.h"
#include "GAS/Effects/GE_OSShieldCooldown.h"
#include "GameplayEffect.h"

UGA_Shield::UGA_Shield()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Shield);
	SetAssetTags(AssetTags);

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Matches your BP screenshot: block activation while already shielded.
	ActivationBlockedTags.AddTag(Tags.Status_Buff_Shield);

	// Default event tag (fired by AN_SendGameplayEvent on the montage).
	TriggerEventTag = Tags.Event_Montage_Trigger;

	// Code-defined defaults (no BP assets required).
	GiveShieldEffectClass = UGE_OSGiveShield::StaticClass();
	CooldownGameplayEffectClass = UGE_OSShieldCooldown::StaticClass();
}

void UGA_Shield::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bTriggered = false;

	// Call base UGameplayAbility activation (but NOT UOSGameplayAbility::ActivateAbility),
	// so we don't auto-Commit at ability start. We commit on the montage event instead.
	UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !ShieldMontage || !GiveShieldEffectClass || !TriggerEventTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Pre-check so we don't play the montage if commit will fail at the trigger point.
	if (!CheckCost(Handle, ActorInfo) || !CheckCooldown(Handle, ActorInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(TEXT("ShieldMontage")),
		ShieldMontage,
		1.0f,
		NAME_None,
		true,
		1.0f
	);

	WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		TriggerEventTag,
		nullptr,
		true,
		true
	);

	if (IsValid(WaitEventTask))
	{
		WaitEventTask->EventReceived.AddDynamic(this, &UGA_Shield::OnTriggerEventReceived);
		WaitEventTask->ReadyForActivation();
	}

	if (IsValid(PlayTask))
	{
		PlayTask->OnBlendOut.AddDynamic(this, &UGA_Shield::OnMontageFinished);
		PlayTask->OnCompleted.AddDynamic(this, &UGA_Shield::OnMontageFinished);
		PlayTask->OnCancelled.AddDynamic(this, &UGA_Shield::OnMontageInterrupted);
		PlayTask->OnInterrupted.AddDynamic(this, &UGA_Shield::OnMontageInterrupted);
		PlayTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGA_Shield::OnTriggerEventReceived(FGameplayEventData Payload)
{
	if (bTriggered)
	{
		return;
	}
	bTriggered = true;

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (*GiveShieldEffectClass)
	{
		ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, GiveShieldEffectClass.GetDefaultObject(), GetAbilityLevel());
	}
}

void UGA_Shield::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Shield::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Shield::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	PlayTask = nullptr;
	WaitEventTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

