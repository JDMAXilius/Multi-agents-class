#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRAbilitySet.generated.h"

class UAbilitySystemComponent;
class UBRGameplayAbility;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FBRAbilitySetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet")
	TSoftClassPtr<UBRGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet")
	int32 AbilityLevel = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct FBRAbilitySetEffectEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet")
	TSoftClassPtr<UGameplayEffect> Effect;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet")
	float EffectLevel = 1.f;
};

UCLASS(BlueprintType, Const, meta = (DisplayName = "BR Ability Set"))
class BREACHPOINT_API UBRAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject, TArray<FGameplayAbilitySpecHandle>& OutAbilityHandles, TArray<FActiveGameplayEffectHandle>& OutEffectHandles) const;

	static void TakeFromAbilitySystem(UAbilitySystemComponent* ASC, TArray<FGameplayAbilitySpecHandle>& AbilityHandles, TArray<FActiveGameplayEffectHandle>& EffectHandles);

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet", meta = (TitleProperty = "Ability"))
	TArray<FBRAbilitySetEntry> Abilities;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet", meta = (TitleProperty = "Effect"))
	TArray<FBRAbilitySetEffectEntry> Effects;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
