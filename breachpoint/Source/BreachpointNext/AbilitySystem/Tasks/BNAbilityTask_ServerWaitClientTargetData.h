#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "BNAbilityTask_ServerWaitClientTargetData.generated.h"

/**
 * The authority's ear for client-produced TargetData. The engine ships tasks that PRODUCE
 * target data (AbilityTask_WaitTargetData) but none that only LISTEN on the server side of
 * the replication channel — that listener lives in Epic's ActionRPG sample, not in
 * GameplayAbilities, so the module carries its own.
 *
 * Runs only on the authority's instance of a LocalPredicted ability whose client sends
 * TargetData via CallServerSetReplicatedTargetData: it binds the ASC's
 * AbilityTargetDataSetDelegate for this spec + activation prediction key, broadcasts each
 * arrival through ValidData, and consumes the replicated data so the ASC's cache never
 * serves a stale claim. TriggerOnce=false keeps it listening for the life of the ability
 * instance (automatic fire); the binding is removed in OnDestroy either way.
 */
UCLASS()
class UBNAbilityTask_ServerWaitClientTargetData : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataDelegate ValidData;

	static UBNAbilityTask_ServerWaitClientTargetData* ServerWaitForClientTargetData(UGameplayAbility* OwningAbility, FName TaskInstanceName, bool bTriggerOnce);

	virtual void Activate() override;

protected:
	virtual void OnDestroy(bool bAbilityEnded) override;

private:
	void OnTargetDataReplicated(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);

	bool bTriggerOnce = false;
};
