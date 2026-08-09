// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "WaitEnhancedInputEvent.generated.h"

class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnhancedInputEventDelegate, FInputActionValue, Value);

UCLASS()
class ZORANSRESISTANCE_API UWaitEnhancedInputEvent : public UAbilityTask
{
	GENERATED_BODY()
	
public:
	
	virtual void Activate() override;
	
	UPROPERTY(BlueprintAssignable)
	FEnhancedInputEventDelegate InputEventReceived;

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"), Category = "Ability Tasks")
	static UWaitEnhancedInputEvent* WaitEnhancedInputEvent(UGameplayAbility* OwningAbility, FName TaskInstanceName, UInputAction* InputAction, ETriggerEvent TriggerEventType, bool bShouldOnlyTriggerOnce = true);
	
private:

	UPROPERTY()
	UInputAction* InputAction;
	
	ETriggerEvent EventType;
	
	bool bTriggerOnce;

	bool bHasBeenTriggered = false;

	void EventReceived(const FInputActionValue& Value);
};