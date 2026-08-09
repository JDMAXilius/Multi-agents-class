#pragma once
// Overrides AllocGameplayEffectContext to return FOSGameplayEffectContext so damage/hit context is carried in GE specs.
#include "AbilitySystemGlobals.h"
#include "OSAbilitySystemGlobals.generated.h"

UCLASS()
class ONSIGHT_API UOSAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()

	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
};