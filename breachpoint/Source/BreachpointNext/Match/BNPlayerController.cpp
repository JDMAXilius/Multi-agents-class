#include "Match/BNPlayerController.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "BreachpointNext.h"
#include "Core/BNGameplayTags.h"
#include "Input/BNInputComponent.h"
#include "Input/BNInputConfig.h"
#include "Match/BNPlayerState.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "GameFramework/Pawn.h"

void ABNPlayerController::SetupInputComponent()
{
	// NEXT owns its input component outright. The project-wide DefaultInputComponentClass
	// names the OLD module's class, and this module depends on nothing there. Creating it
	// before Super is the engine's own sanctioned override point.
	if (!InputComponent)
	{
		InputComponent = NewObject<UBNInputComponent>(this, TEXT("BNInputComponent0"));
		InputComponent->RegisterComponent();
	}

	Super::SetupInputComponent();

	if (!IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (MappingContexts.IsEmpty())
			{
				UE_LOG(LogBN, Error, TEXT("BNPlayerController: MappingContexts is empty — set +MappingContexts under [/Script/BreachpointNext.BNPlayerController] in DefaultGame.ini. No key reaches any action."));
			}
			for (int32 Index = 0; Index < MappingContexts.Num(); ++Index)
			{
				if (UInputMappingContext* Context = MappingContexts[Index].LoadSynchronous())
				{
					Subsystem->AddMappingContext(Context, Index);
				}
				else
				{
					UE_LOG(LogBN, Error, TEXT("BNPlayerController: MappingContexts[%d] '%s' failed to load."), Index, *MappingContexts[Index].ToString());
				}
			}
		}
	}

	UBNInputComponent* BNInput = Cast<UBNInputComponent>(InputComponent);
	const UBNInputConfig* Config = InputConfig.LoadSynchronous();
	if (!BNInput || !Config)
	{
		UE_LOG(LogBN, Error, TEXT("BNPlayerController: input not bound (component=%s, InputConfig='%s'). Run Tools/bn/10_input_assets.py and check DefaultGame.ini."),
			BNInput ? TEXT("ok") : TEXT("wrong class"), *InputConfig.ToString());
		return;
	}

	const auto Bind = [this, BNInput, Config](FGameplayTag Tag, ETriggerEvent Event, auto Func)
	{
		if (!BNInput->BindActionByTag(Config, Tag, Event, this, Func))
		{
			UE_LOG(LogBN, Error, TEXT("BNPlayerController: %s has no InputAction in '%s' — that control is dead."), *Tag.ToString(), *InputConfig.ToString());
		}
	};

	Bind(BNTags::Input_Move, ETriggerEvent::Triggered, &ABNPlayerController::HandleMove);
	Bind(BNTags::Input_Look, ETriggerEvent::Triggered, &ABNPlayerController::HandleLook);
	Bind(BNTags::Input_Jump, ETriggerEvent::Started, &ABNPlayerController::HandleJumpPressed);
	Bind(BNTags::Input_Jump, ETriggerEvent::Completed, &ABNPlayerController::HandleJumpReleased);
	Bind(BNTags::Input_Crouch, ETriggerEvent::Started, &ABNPlayerController::HandleCrouchPressed);
	Bind(BNTags::Input_Crouch, ETriggerEvent::Completed, &ABNPlayerController::HandleCrouchReleased);
	// Press only: the swap verbs are one-shot, so there is no release event to forward.
	Bind(BNTags::Input_Weapon_Next, ETriggerEvent::Started, &ABNPlayerController::HandleWeaponNextPressed);
	Bind(BNTags::Input_Weapon_Previous, ETriggerEvent::Started, &ABNPlayerController::HandleWeaponPreviousPressed);
}

void ABNPlayerController::HandleMove(const FInputActionValue& Value)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotationMatrix YawMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f));
	ControlledPawn->AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	ControlledPawn->AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void ABNPlayerController::HandleLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddYawInput(Axis.X);
	AddPitchInput(Axis.Y);
}

void ABNPlayerController::HandleJumpPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Jump);
	}
}

void ABNPlayerController::HandleJumpReleased()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(BNTags::Input_Jump);
	}
}

void ABNPlayerController::HandleCrouchPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Crouch);
	}
}

void ABNPlayerController::HandleCrouchReleased()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(BNTags::Input_Crouch);
	}
}

void ABNPlayerController::HandleWeaponNextPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Weapon_Next);
	}
}

void ABNPlayerController::HandleWeaponPreviousPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Weapon_Previous);
	}
}

UBNAbilitySystemComponent* ABNPlayerController::GetBNAbilitySystemComponent() const
{
	const ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	return PS ? PS->GetBNAbilitySystemComponent() : nullptr;
}
