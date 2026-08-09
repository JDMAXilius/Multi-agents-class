// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Server-initiated death: cancels abilities, plays death montage, then routes into Character death handling.

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "Data/OSHitDamageContext.h"
#include "GA_OSDeath.generated.h"

class UGameplayEffect;
class UAnimMontage;
class UChooserTable;

UCLASS()
class ONSIGHT_API UGA_OSDeath : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSDeath();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// --- Configuration ---

	UPROPERTY(EditDefaultsOnly, Category="Death")
	TSubclassOf<UGameplayEffect> DeathStateEffectClass;

	/* Fallback montage used when the chooser table is not set or returns nothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	/* Chooser table for direction-based death montage selection.
	   Uses FOSHitReacts with ReactType=Death and HitDirection from the killing blow.
	   Falls back to DeathMontage when null or when no row matches. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Death")
	TObjectPtr<UChooserTable> DeathChooser;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Death", meta=(ClampMin="0.1", ClampMax="5.0"))
	float DeathMontagePlayRate = 1.0f;

	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

private:
	void FinishDeath();

	UFUNCTION()
	void OnDeathMontageCompleted();

	UFUNCTION()
	void OnDeathMontageBlendOut();

	UFUNCTION()
	void OnDeathMontageInterrupted();

	UFUNCTION()
	void OnDeathMontageCancelled();

	// --- Cached State ---

	UPROPERTY()
	FOSDeathEventInfo CachedDeathEvent;

	FTimerHandle DeathTimerHandle;

	bool bFinishDeathCalled = false;
	bool bDeathActivated = false;
};

