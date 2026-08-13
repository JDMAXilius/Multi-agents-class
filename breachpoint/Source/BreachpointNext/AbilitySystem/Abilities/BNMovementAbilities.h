#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "BNMovementAbilities.generated.h"

UCLASS()
class BREACHPOINTNEXT_API UBNGA_Jump : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnLanded(const FHitResult& Hit);

	UFUNCTION()
	void OnInputRelease(float TimeHeld);

	FActiveGameplayEffectHandle JumpingHandle;
	FActiveGameplayEffectHandle InAirHandle;
};

UCLASS()
class BREACHPOINTNEXT_API UBNGA_Crouch : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
