#include "Character/BRCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Character/BRCharacterMovementComponent.h"
#include "Core/BRCore.h"
#include "Input/BRInputComponent.h"
#include "Input/BRInputConfig.h"
#include "Match/BRPlayerController.h"
#include "Match/BRPlayerState.h"

ABRCharacter::ABRCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	if (USkeletalMeshComponent* Mesh3P = GetMesh())
	{
		Mesh3P->SetOwnerNoSee(true);
		Mesh3P->CastShadow = true;
		Mesh3P->bCastDynamicShadow = true;
		Mesh3P->bCastHiddenShadow = true;
		Mesh3P->bReceivesDecals = false;
		Mesh3P->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
		Mesh3P->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
		Mesh3P->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}
}

UAbilitySystemComponent* ABRCharacter::GetAbilitySystemComponent() const
{
	const ABRPlayerState* BRPlayerState = GetPlayerState<ABRPlayerState>();
	return BRPlayerState ? BRPlayerState->GetAbilitySystemComponent() : nullptr;
}

void ABRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
}

void ABRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystem();
}

void ABRCharacter::InitializeAbilitySystem()
{
	ABRPlayerState* BRPlayerState = GetPlayerState<ABRPlayerState>();
	if (!BRPlayerState)
	{
		return;
	}

	UBRAbilitySystemComponent* ASC = BRPlayerState->GetBRAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}
	ASC->InitAbilityActorInfo(BRPlayerState, this);

	if (!HasAuthority())
	{
		return;
	}
	ASC->ApplyInitStats();
	BRPlayerState->GiveStartupLoadout();
}

UBRCharacterMovementComponent* ABRCharacter::GetBRCharacterMovement() const
{
	return Cast<UBRCharacterMovementComponent>(GetCharacterMovement());
}

void ABRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABRCharacter::MoveInput);

		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABRCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ABRCharacter::LookInput);
	}
}

void ABRCharacter::MoveInput(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	DoMove(MovementVector.X, MovementVector.Y);
}

void ABRCharacter::LookInput(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	DoAim(LookAxisVector.X, LookAxisVector.Y);
}

void ABRCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ABRCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}
