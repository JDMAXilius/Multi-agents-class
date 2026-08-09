#include "Character/BPCharacter.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "FPS/BPAnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Match/BPPlayerState.h"

ABPCharacter::ABPCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.f);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName(TEXT("NoCollision")));
	FirstPersonMesh->SetAnimInstanceClass(UBPAnimInstance::StaticClass());

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName(TEXT("head")));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	if (USkeletalMeshComponent* Mesh3P = GetMesh())
	{
		Mesh3P->SetOwnerNoSee(true);
		Mesh3P->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
		Mesh3P->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
		Mesh3P->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		Mesh3P->SetAnimInstanceClass(UBPAnimInstance::StaticClass());
	}

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

UAbilitySystemComponent* ABPCharacter::GetAbilitySystemComponent() const
{
	const ABPPlayerState* PS = GetPlayerState<ABPPlayerState>();
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

void ABPCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
}

void ABPCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystem();
}

void ABPCharacter::InitializeAbilitySystem()
{
	ABPPlayerState* PS = GetPlayerState<ABPPlayerState>();
	if (!PS)
	{
		return;
	}

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	ASC->InitAbilityActorInfo(PS, this);
}

const UInputAction* ABPCharacter::ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* Name)
{
	if (SoftAction.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BPCharacter: '%s' is unset — pin under [/Script/Breachpoint.BPCharacter] in DefaultGame.ini."), Name);
		return nullptr;
	}

	const UInputAction* Action = SoftAction.LoadSynchronous();
	if (!Action)
	{
		UE_LOG(LogTemp, Error, TEXT("BPCharacter: '%s' path '%s' failed to load."), Name, *SoftAction.ToSoftObjectPath().ToString());
	}
	return Action;
}

void ABPCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("BPCharacter: InputComponent is not EnhancedInput."));
		return;
	}

	if (const UInputAction* Move = ResolveAction(MoveAction, TEXT("MoveAction")))
	{
		EnhancedInputComponent->BindAction(Move, ETriggerEvent::Triggered, this, &ABPCharacter::MoveInput);
	}

	if (const UInputAction* Look = ResolveAction(LookAction, TEXT("LookAction")))
	{
		EnhancedInputComponent->BindAction(Look, ETriggerEvent::Triggered, this, &ABPCharacter::LookInput);
	}
	if (const UInputAction* MouseLook = ResolveAction(MouseLookAction, TEXT("MouseLookAction")))
	{
		EnhancedInputComponent->BindAction(MouseLook, ETriggerEvent::Triggered, this, &ABPCharacter::LookInput);
	}
}

void ABPCharacter::MoveInput(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void ABPCharacter::LookInput(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoAim(LookAxisVector.X, LookAxisVector.Y);
}

void ABPCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ABPCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ABPCharacter::DoJumpStart()
{
	Jump();
}

void ABPCharacter::DoJumpEnd()
{
	StopJumping();
}
