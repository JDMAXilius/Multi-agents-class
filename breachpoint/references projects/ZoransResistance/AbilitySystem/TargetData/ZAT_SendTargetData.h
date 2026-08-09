// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ZoransResistance/AbilitySystem/TargetData/ZoransTargetActor.h"
#include "ZAT_SendTargetData.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitTargetDataReusableDelegate, const FGameplayAbilityTargetDataHandle&, Data);

UCLASS()
class ZORANSRESISTANCE_API UZAT_SendTargetData : public UAbilityTask
{
	GENERATED_BODY()
	public:
	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", HideSpawnParms = "Instigator"), Category = "Ability|Tasks")
		static UZAT_SendTargetData* SendTargetData(
			UGameplayAbility* OwningAbility,
			FName TaskInstanceName,
			TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType,
			AZoransTargetActor* InTargetActor,
			bool bCreateKeyIfNotValidForMorePredicting = false
		);
	
	FWaitTargetDataReusableDelegate ValidData;
	
	FWaitTargetDataReusableDelegate Cancelled;

	virtual void Activate() override;

	UFUNCTION()
	virtual void OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);

	UFUNCTION()
	virtual void OnTargetDataReplicatedCancelledCallback();

	UFUNCTION()
	virtual void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	virtual void OnTargetDataCancelledCallback(const FGameplayAbilityTargetDataHandle& Data);

	// Called when the ability is asked to confirm from an outside node. What this means depends on the individual task. By default, this does nothing other than ending if bEndTask is true.
	virtual void ExternalConfirm(bool bEndTask) override;

	// Called when the ability is asked to cancel from an outside node. What this means depends on the individual task. By default, this does nothing other than ending the task.
	virtual void ExternalCancel() override;

protected:
	UPROPERTY()
	AZoransTargetActor* TargetActor;

	bool bCreateKeyIfNotValidForMorePredicting;

	TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType;

	FDelegateHandle OnTargetDataReplicatedCallbackDelegateHandle;

	virtual void InitializeTargetActor() const;
	virtual void FinalizeTargetActor() const;

	virtual void RegisterTargetDataCallbacks();

	virtual void OnDestroy(bool AbilityEnded) override;

	virtual bool ShouldReplicateDataToServer() const;
};
