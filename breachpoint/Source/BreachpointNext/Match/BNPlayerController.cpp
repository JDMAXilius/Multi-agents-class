#include "Match/BNPlayerController.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "Core/BNGameplayTags.h"
#include "Match/BNPlayerState.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "GameFramework/Pawn.h"

void ABNPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	MoveAction = NewObject<UInputAction>(this);
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction = NewObject<UInputAction>(this);
	LookAction->ValueType = EInputActionValueType::Axis2D;
	JumpAction = NewObject<UInputAction>(this);
	JumpAction->ValueType = EInputActionValueType::Boolean;
	CrouchAction = NewObject<UInputAction>(this);
	CrouchAction->ValueType = EInputActionValueType::Boolean;

	DefaultMappingContext = NewObject<UInputMappingContext>(this);
	{
		FEnhancedActionKeyMapping& Mapping = DefaultMappingContext->MapKey(MoveAction, EKeys::W);
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	}
	{
		FEnhancedActionKeyMapping& Mapping = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	DefaultMappingContext->MapKey(MoveAction, EKeys::D);
	{
		FEnhancedActionKeyMapping& Mapping = DefaultMappingContext->MapKey(MoveAction, EKeys::A);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	{
		FEnhancedActionKeyMapping& Mapping = DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);
		UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(this);
		NegateY->bX = false;
		NegateY->bY = true;
		NegateY->bZ = false;
		Mapping.Modifiers.Add(NegateY);
	}
	DefaultMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	DefaultMappingContext->MapKey(CrouchAction, EKeys::LeftControl);

	if (IsLocalController())
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABNPlayerController::HandleMove);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABNPlayerController::HandleLook);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ABNPlayerController::HandleJumpPressed);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABNPlayerController::HandleJumpReleased);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABNPlayerController::HandleCrouchPressed);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ABNPlayerController::HandleCrouchReleased);
	}
}

void ABNPlayerController::HandleMove(const FInputActionValue& Value)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	ControlledPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis.Y);
	ControlledPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis.X);
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

UBNAbilitySystemComponent* ABNPlayerController::GetBNAbilitySystemComponent() const
{
	const ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	return PS ? PS->GetBNAbilitySystemComponent() : nullptr;
}
