// Copyright Zollpa LLC.


#include "WaitEnhancedInputEvent.h"
#include "GameFramework/Character.h"

void UWaitEnhancedInputEvent::Activate()
{
	if (!AbilitySystemComponent.Get() || !Ability || !InputAction)
	{
		return;
	}

	if (const ACharacter* AvatarCharacter = Cast<ACharacter>(Ability->GetAvatarActorFromActorInfo()))
	{
		if (const APlayerController* PlayerController = Cast<APlayerController>(AvatarCharacter->GetController()))
		{
			if (UEnhancedInputComponent* PlayerEnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
			{
				PlayerEnhancedInputComponent->BindAction(InputAction, EventType, this, &UWaitEnhancedInputEvent::EventReceived);
			}
		}
	}
}

UWaitEnhancedInputEvent* UWaitEnhancedInputEvent::WaitEnhancedInputEvent(UGameplayAbility* OwningAbility, const FName TaskInstanceName, UInputAction* InputAction, const ETriggerEvent TriggerEventType, const bool bShouldOnlyTriggerOnce)
{
	UWaitEnhancedInputEvent* AbilityTask = NewAbilityTask<UWaitEnhancedInputEvent>(OwningAbility, TaskInstanceName);
	
	AbilityTask->InputAction = InputAction;
	AbilityTask->EventType = TriggerEventType;
	AbilityTask->bTriggerOnce = bShouldOnlyTriggerOnce;
	
	return AbilityTask;
}

void UWaitEnhancedInputEvent::EventReceived(const FInputActionValue& Value)
{
	if (bTriggerOnce && bHasBeenTriggered)
	{
		return;
	}

	bHasBeenTriggered = true;
	InputEventReceived.Broadcast(Value);
}