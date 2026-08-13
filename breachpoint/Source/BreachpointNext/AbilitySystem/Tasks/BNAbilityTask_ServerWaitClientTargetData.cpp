#include "AbilitySystem/Tasks/BNAbilityTask_ServerWaitClientTargetData.h"

#include "AbilitySystemComponent.h"

UBNAbilityTask_ServerWaitClientTargetData* UBNAbilityTask_ServerWaitClientTargetData::ServerWaitForClientTargetData(UGameplayAbility* OwningAbility, FName TaskInstanceName, bool bTriggerOnce)
{
	UBNAbilityTask_ServerWaitClientTargetData* Task = NewAbilityTask<UBNAbilityTask_ServerWaitClientTargetData>(OwningAbility, TaskInstanceName);
	Task->bTriggerOnce = bTriggerOnce;
	return Task;
}

void UBNAbilityTask_ServerWaitClientTargetData::Activate()
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!Ability || !ASC)
	{
		EndTask();
		return;
	}

	ASC->AbilityTargetDataSetDelegate(GetAbilitySpecHandle(), GetActivationPredictionKey())
		.AddUObject(this, &UBNAbilityTask_ServerWaitClientTargetData::OnTargetDataReplicated);
}

void UBNAbilityTask_ServerWaitClientTargetData::OnTargetDataReplicated(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag)
{
	// Copy before consuming: ConsumeClientReplicatedTargetData clears the ASC's cached
	// handle, and Data is a reference into that cache.
	const FGameplayAbilityTargetDataHandle LocalData = Data;
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		ASC->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		ValidData.Broadcast(LocalData);
	}

	if (bTriggerOnce)
	{
		EndTask();
	}
}

void UBNAbilityTask_ServerWaitClientTargetData::OnDestroy(bool bAbilityEnded)
{
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		ASC->AbilityTargetDataSetDelegate(GetAbilitySpecHandle(), GetActivationPredictionKey()).RemoveAll(this);
	}

	Super::OnDestroy(bAbilityEnded);
}
