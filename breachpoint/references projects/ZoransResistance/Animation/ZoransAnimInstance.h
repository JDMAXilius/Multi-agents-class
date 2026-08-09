// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Animation/AnimInstance.h"
#include "ZoransAnimInstance.generated.h"

UCLASS()
class ZORANSRESISTANCE_API UZoransAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	void InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

protected:

	virtual void NativeInitializeAnimation() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif // WITH_EDITOR
	
	// Gameplay tags that can be mapped to blueprint variables. The variables will automatically update as the tags are added or removed.
	// These should be used instead of manually querying for the gameplay tags.
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;
};