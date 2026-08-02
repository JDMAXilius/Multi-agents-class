#pragma once

#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "CoreMinimal.h"

#include "BRGA_Jump.generated.h"

class ACharacter;

UCLASS(meta = (DisplayName = "BRGA_Jump"))
class BREACHPOINT_API UBRGA_Jump : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Jump(const FObjectInitializer& ObjectInitializer);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	ACharacter* GetAvatarCharacter(const FGameplayAbilityActorInfo* ActorInfo) const;
};
