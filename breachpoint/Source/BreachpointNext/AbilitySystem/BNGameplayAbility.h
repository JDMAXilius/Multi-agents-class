#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BNGameplayAbility.generated.h"

UCLASS()
class BREACHPOINTNEXT_API UBNGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBNGameplayAbility();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	FActiveGameplayEffectHandle ApplyStateTag(FGameplayTag Tag);
	void RemoveStateTag(FActiveGameplayEffectHandle& Handle);
};
