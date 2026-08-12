#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "Templates/SubclassOf.h"
#include "BNAbilitySet.generated.h"

class UBNAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

USTRUCT()
struct FBNAbilitySetAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly)
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly)
	int32 Level = 1;
};

USTRUCT()
struct FBNAbilitySetHandles
{
	GENERATED_BODY()

	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	TArray<FActiveGameplayEffectHandle> EffectHandles;
};

UCLASS()
class BREACHPOINTNEXT_API UBNAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	void GiveToAbilitySystem(UBNAbilitySystemComponent* ASC, FBNAbilitySetHandles& OutHandles) const;
	void TakeFromAbilitySystem(UBNAbilitySystemComponent* ASC, FBNAbilitySetHandles& Handles) const;

	UPROPERTY(EditDefaultsOnly)
	TArray<FBNAbilitySetAbility> Abilities;

	UPROPERTY(EditDefaultsOnly)
	TArray<TSubclassOf<UGameplayEffect>> Effects;
};
