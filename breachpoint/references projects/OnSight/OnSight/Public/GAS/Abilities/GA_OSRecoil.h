#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GA_OSRecoil.generated.h"

class UAbilityTask_PlayMontageAndWait;

/**
 * Server-authoritative recoil: triggered by GameplayEvent.Effect.Recoil when an attacker's
 * hit is fully absorbed by a block. Cancels the active combo, blocks all abilities, and
 * plays a stagger montage — creating a counterattack window for the defender.
 */
UCLASS()
class ONSIGHT_API UGA_OSRecoil : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSRecoil();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Recoil/stagger montage (~0.5-0.8s). Assign in class defaults. */
	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	TObjectPtr<UAnimMontage> RecoilMontage = nullptr;

private:
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
