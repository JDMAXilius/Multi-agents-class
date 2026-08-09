// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Shield: grants shield buff GE and plays cue. Cooldown applied by GE_OSShieldCooldown (Cooldown.Shield tag).

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_Shield.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

/**
 * Shield ability (matches BP flow):
 * Play montage -> Wait GameplayEvent (AnimNotify) -> Commit -> Apply GE_GiveShield
 */
UCLASS()
class ONSIGHT_API UGA_Shield : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Shield();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Montage to play when starting shield */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shield")
	TObjectPtr<UAnimMontage> ShieldMontage = nullptr;

	/** Event tag fired by AnimNotify (ex: GameplayEvent.Montage.Trigger) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shield")
	FGameplayTag TriggerEventTag;

	/** GameplayEffect applied on successful commit (GE_GiveShield) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shield")
	TSubclassOf<UGameplayEffect> GiveShieldEffectClass;

private:
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> PlayTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask;

	bool bTriggered = false;

	UFUNCTION()
	void OnTriggerEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageInterrupted();
};

