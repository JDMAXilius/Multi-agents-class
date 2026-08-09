// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GA_OSFrostBolt.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class AOSProjectile;

/**
 *  Mid-range frost projectile.
 *  Plays a cast montage, spawns an AOSProjectile on anim notify, then ends.
 *  The projectile Blueprint owns its own speed, lifetime, VFX, collision, and payload effects.
 */
UCLASS()
class ONSIGHT_API UGA_OSFrostBolt : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSFrostBolt();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// ========== Projectile ==========

	/** Projectile Blueprint class to spawn (handles its own speed, lifetime, VFX, and payload) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frost Bolt")
	TSubclassOf<AOSProjectile> ProjectileClass;

	/** Spawn offset forward from the character */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frost Bolt")
	float SpawnForwardOffset = 100.0f;

	// ========== Animation ==========

	/** Cast montage (should have an AN_OSDirectDamage notify to trigger the projectile spawn) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Frost Bolt")
	TObjectPtr<UAnimMontage> CastMontage;

private:

	/** Called when the montage's DirectDamage event fires — spawns the projectile */
	UFUNCTION()
	void OnSpawnEventReceived(FGameplayEventData Payload);

	/** Called when the montage completes or is interrupted */
	UFUNCTION()
	void OnMontageFinished();
};
