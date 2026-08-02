// Breachpoint. The pawn: a body, not a brain.

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

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

ABRCharacter::ABRCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	// Mesh3P — the inherited ACharacter mesh, configured as the full body. See GetMesh3P()'s
	// comment for why this is the inherited component and not a second one.
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

// ---------------------------------------------------------------------------
// The GAS init dance (§3.4) — BOTH halves, and the reason there are two
// ---------------------------------------------------------------------------

void ABRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem(TEXT("PossessedBy (server)"));
}

void ABRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystem(TEXT("OnRep_PlayerState (client)"));
}

void ABRCharacter::InitializeAbilitySystem(const TCHAR* CallSite)
{
	if (!BRGas::IsStageEnabled(EBRGasStage::AttributesOnly))
	{
		UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacter '%s': %s — GAS stage gate is '%s'; ASC init skipped (needs at least 'AttributesOnly'). Set GasStage in Config/DefaultGame.ini."),
			*GetName(), CallSite, BRGas::ToString(BRGas::GetStage()));
		return;
	}

	ABRPlayerState* BRPlayerState = GetPlayerState<ABRPlayerState>();
	if (!BRPlayerState)
	{
		UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacter '%s': %s — no ABRPlayerState yet; the other init path will cover it."),
			*GetName(), CallSite);
		return;
	}

	UBRAbilitySystemComponent* ASC = BRPlayerState->GetBRAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRCharacter '%s': %s — ABRPlayerState '%s' has no ASC. No ability on this pawn will ever activate."),
			*GetName(), CallSite, *BRPlayerState->GetName());
		return;
	}
	ASC->InitAbilityActorInfo(BRPlayerState, this);

	UE_LOG(LogBRCombat, Log, TEXT("BRCharacter '%s': %s — InitAbilityActorInfo(owner='%s', avatar='%s')."),
		*GetName(), CallSite, *BRPlayerState->GetName(), *GetName());

	if (!HasAuthority())
	{
		return;
	}
	ASC->ApplyInitStats();
}

UBRCharacterMovementComponent* ABRCharacter::GetBRCharacterMovement() const
{
	return Cast<UBRCharacterMovementComponent>(GetCharacterMovement());
}

void ABRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ABRCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABRCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABRCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABRCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ABRCharacter::LookInput);
	}
}


void ABRCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void ABRCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void ABRCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ABRCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ABRCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void ABRCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
