// Copyright Zollpa LLC

#include "ZoransAnimInstance.h"
#include "AbilitySystemGlobals.h"

void UZoransAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	check(InAbilitySystemComponent);

	GameplayTagPropertyMap.Initialize(this, InAbilitySystemComponent);
}

void UZoransAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (const AActor* OwningActor = GetOwningActor())
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
		{
			InitializeWithAbilitySystem(AbilitySystemComponent);
		}
	}
}

#if WITH_EDITOR
EDataValidationResult UZoransAnimInstance::IsDataValid(TArray<FText>& ValidationErrors)
{
	Super::IsDataValid(ValidationErrors);

	GameplayTagPropertyMap.IsDataValid(this, ValidationErrors);

	return ((ValidationErrors.Num() > 0) ? EDataValidationResult::Invalid : EDataValidationResult::Valid);
}
#endif // WITH_EDITOR