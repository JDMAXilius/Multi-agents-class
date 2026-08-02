#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagContainer.h"
#include "InputTriggers.h"

#include "Input/BRInputConfig.h"

#include "BRInputComponent.generated.h"

UCLASS(meta = (DisplayName = "BR Input Component"))
class BREACHPOINT_API UBRInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	UBRInputComponent(const FObjectInitializer& ObjectInitializer);

	template<class UserClass, typename FuncType>
	static int32 BindNativeAction(UEnhancedInputComponent* InputComponent, const UBRInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bEnsureIfNotFound = true);

	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	static void BindAbilityActions(UEnhancedInputComponent* InputComponent, const UBRInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>* BindHandles = nullptr, ETriggerEvent PressedEvent = ETriggerEvent::Triggered, ETriggerEvent ReleasedEvent = ETriggerEvent::Completed);

	void RemoveBinds(TArray<uint32>& BindHandles);
};

template<class UserClass, typename FuncType>
int32 UBRInputComponent::BindNativeAction(UEnhancedInputComponent* InputComponent, const UBRInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bEnsureIfNotFound)
{
	if (!ensureMsgf(InputComponent != nullptr, TEXT("BRInputComponent::BindNativeAction called with no input component; every key on this pawn is dead.")))
	{
		return 0;
	}

	if (!ensureMsgf(InputConfig != nullptr, TEXT("BRInputComponent::BindNativeAction called with no input config; every key on this pawn is dead.")))
	{
		return 0;
	}

	int32 Bound = 0;
	for (const FBRInputAction& Row : InputConfig->GetNativeInputActions())
	{
		if (Row.InputTag != InputTag)
		{
			continue;
		}

		if (const UInputAction* Action = Row.GetInputAction())
		{
			InputComponent->BindAction(Action, TriggerEvent, Object, Func);
			++Bound;
		}
	}

	if (Bound == 0 && bEnsureIfNotFound)
	{
		ensureMsgf(false, TEXT("BRInputComponent::BindNativeAction: no native InputAction for tag '%s'."), *InputTag.ToString());
	}

	return Bound;
}

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void UBRInputComponent::BindAbilityActions(UEnhancedInputComponent* InputComponent, const UBRInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>* BindHandles, ETriggerEvent PressedEvent, ETriggerEvent ReleasedEvent)
{
	if (!ensureMsgf(InputComponent != nullptr, TEXT("BRInputComponent::BindAbilityActions called with no input component; no ability is reachable from hardware.")))
	{
		return;
	}

	if (!ensureMsgf(InputConfig != nullptr, TEXT("BRInputComponent::BindAbilityActions called with no input config; no ability is reachable from hardware.")))
	{
		return;
	}

	for (const FBRInputAction& Row : InputConfig->GetAbilityInputActions())
	{
		if (!Row.InputTag.IsValid())
		{
			continue;
		}

		const UInputAction* Action = Row.GetInputAction();
		if (!Action)
		{
			ensureMsgf(false, TEXT("BRInputComponent: ability row '%s' has no loadable InputAction; that ability is unreachable from hardware."), *Row.InputTag.ToString());
			continue;
		}

		if (PressedFunc)
		{
			FEnhancedInputActionEventBinding& Binding = InputComponent->BindAction(Action, PressedEvent, Object, PressedFunc, Row.InputTag);
			if (BindHandles)
			{
				BindHandles->Add(Binding.GetHandle());
			}
		}

		if (ReleasedFunc)
		{
			FEnhancedInputActionEventBinding& Binding = InputComponent->BindAction(Action, ReleasedEvent, Object, ReleasedFunc, Row.InputTag);
			if (BindHandles)
			{
				BindHandles->Add(Binding.GetHandle());
			}
		}
	}
}
