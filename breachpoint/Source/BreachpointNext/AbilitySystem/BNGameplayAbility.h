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
	FActiveGameplayEffectHandle ApplyStateTag(FGameplayTag Tag);
	void RemoveStateTag(FActiveGameplayEffectHandle& Handle);
};
