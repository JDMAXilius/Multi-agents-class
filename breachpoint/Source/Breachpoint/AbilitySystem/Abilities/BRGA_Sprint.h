// Breachpoint. Sprint: the WhileHeld prover and the first predicted movement state.
#pragma once

#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "CoreMinimal.h"

#include "BRGA_Sprint.generated.h"

class UBRCharacterMovementComponent;

UCLASS(meta = (DisplayName = "BRGA_Sprint"))
class BREACHPOINT_API UBRGA_Sprint : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Sprint(const FObjectInitializer& ObjectInitializer);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UBRCharacterMovementComponent* GetBRCharacterMovement(const FGameplayAbilityActorInfo* ActorInfo) const;
};
