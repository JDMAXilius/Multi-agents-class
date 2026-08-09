// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
/* Fire-element reaction ability: triggered by Event.Dodge.Started, listens for
   Event.Dodge.Ended, then spawns a flame trail spanning the dash path. Does NOT
   move the character - the base dodge owns all movement. */

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GA_OSFlameDash.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitDelay;

UCLASS()
class ONSIGHT_API UGA_OSFlameDash : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSFlameDash();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// Hard cap on the ability's lifetime. Covers the case where Event.Dodge.Ended never fires
	// (e.g. dodge cancelled by another ability before EndAbility runs). Set generously above the
	// longest dash duration.
	UPROPERTY(EditDefaultsOnly, Category = "FlameDash")
	float MaxLifetime = 1.0f;

private:
	void SpawnTrail(const FVector& StartLocation, const FVector& EndLocation);

	UFUNCTION()
	void OnDodgeEnded(FGameplayEventData Payload);

	UFUNCTION()
	void OnMaxLifetimeElapsed();

	FVector CachedStartLocation = FVector::ZeroVector;

	UPROPERTY() TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitDodgeEndedTask;
	UPROPERTY() TObjectPtr<UAbilityTask_WaitDelay> MaxLifetimeTask;
};
