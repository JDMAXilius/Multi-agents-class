#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "BNAbilitySystemComponent.generated.h"

UCLASS()
class BREACHPOINTNEXT_API UBNAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
};
