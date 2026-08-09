// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/ShooterCharacter.h"
#include "InventorySystem/Weapon.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InventorySystem/InteractableInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "Characters/Components/NMWeaponComponent.h"

// Sets default values
AShooterCharacter::AShooterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MaxWalkSpeedSprinting(1000.f)
	, bAiming(false)
	// Camera field of view values
	, CameraDefaultFOV(0.f) // Set in BeginPlay
	, CameraCurrentFOV(0.f) // Set in BeginPlay
	, CameraZoomedFOV(50.f)
	, ZoomInterpSpeed(20.f)
	// Hover Settings
	, HoverTurnRate(FRotator(0.0f, 250.f, 0.0f))
	, HoverGravityScale(0.f)
	, HoverAirControl(0.9f)
	, HoverDeceleration(350.f)
	, HoverAcceleration(1024.f)
	, HoverWalkingSpeed(800.f)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


	// Mesh that will be retarget from Manny
	RetargetedMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RetargetedMesh"));
	RetargetedMesh->SetupAttachment(GetMesh());

	// Don't rotate when the controller rotates. 
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	// Configure character movement
	 GetCharacterMovement()->bOrientRotationToMovement = false;
	 GetCharacterMovement()->RotationRate = FRotator{ 0.f, 540.f, 0.f };
	 GetMovementComponent()->GetNavAgentPropertiesRef().bCanCrouch = true;
}

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Cache the default walk speed
	GetCharacterMovement()->MaxCustomMovementSpeed = GetCharacterMovement()->MaxWalkSpeed;

	if (GetCamera())
	{
		// Cache default FOV
		CameraDefaultFOV = GetCamera()->FieldOfView;
		CameraCurrentFOV = CameraDefaultFOV;
	}

	// Spawn the AR weapon and equip it to the mesh
	InventorySlot1();

	RecordOriginalSettings();
}

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	// Handle interpolation for zoom when aiming
	CameraInterpZoom(DeltaTime);

	DescentPlayer(DeltaTime);

	if (GetCharacterMovement()->IsMovingOnGround() && bHoverOnCooldown)
	{
		EndHoverCooldown();
	}

	if (GetCharacterMovement()->IsMovingOnGround())
	{
		if (bIsHovering)
		{
			OnStopHover();
		}

		if (bHoverOnCooldown)
		{
			bHoverOnCooldown = false;
		}
	}
}

void AShooterCharacter::Move(const FInputActionValue& Value)
{
	// Get input data
	const auto MovementVector = Value.Get<FVector2d>();

	// Get the controller yaw rotation
	const auto Rotation{ Controller->GetControlRotation() };

	// Get yaw rotation matrix
	const auto YawRotationMatrix = FRotationMatrix{ {0.f, Rotation.Yaw, 0.f} };

	// Get the forward vector
	const auto ForwardDirection{ YawRotationMatrix.GetUnitAxis(EAxis::X) };
	AddMovementInput(ForwardDirection, MovementVector.Y);

	// Get the right vector
	const auto RightDirection{ YawRotationMatrix.GetUnitAxis(EAxis::Y) };
	AddMovementInput(RightDirection, MovementVector.X);
}

void AShooterCharacter::Look(const FInputActionValue& Value)
{
	// Get input data
	const auto MouseMovementVector = Value.Get<FVector2d>();

	// Rotate the camera in it's respective axis, using the mouse input
	AddControllerYawInput(MouseMovementVector.X);
	AddControllerPitchInput(MouseMovementVector.Y);
}

void AShooterCharacter::Sprint()
{
	GetCharacterMovement()->MaxWalkSpeed = MaxWalkSpeedSprinting;
}

void AShooterCharacter::StopSprinting()
{
	GetCharacterMovement()->MaxWalkSpeed = GetCharacterMovement()->MaxCustomMovementSpeed;
}

void AShooterCharacter::Fire()
{
	WeaponComponent->FireWeapon();
}

void AShooterCharacter::Interact()
{
	TArray<AActor*> OverlappingActors;
	GetCapsuleComponent()->GetOverlappingActors(OverlappingActors);

	for (auto& OverlappingActor : OverlappingActors)
	{
		if (OverlappingActor->Implements<UInteractableInterface>())
		{
			IInteractableInterface::Execute_Interact(OverlappingActor, this);
			break;
		}
	}
}

void AShooterCharacter::StartHover()
{
	if (!CanStartHover()) 
	{
		return;
	}

	OnStartHover();
	bIsHovering = true;
	CurrentVelocity = GetCharacterMovement()->Velocity;

	GetCharacterMovement()->RotationRate = HoverTurnRate;
	GetCharacterMovement()->GravityScale = HoverGravityScale;
	GetCharacterMovement()->AirControl = HoverAirControl;
	GetCharacterMovement()->BrakingDecelerationFalling = HoverDeceleration;
	GetCharacterMovement()->MaxAcceleration = HoverAcceleration;
	GetCharacterMovement()->MaxWalkSpeed = HoverWalkingSpeed;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;

	GetWorldTimerManager().SetTimer(HoverDurationTimer, this, &AShooterCharacter::StopHover, MaxHoverDuration, false);
}

void AShooterCharacter::StopHover()
{
	OnStopHover();
	bIsHovering = false;
	ApplyOriginalSettings();
	// Clear hover duration timer if it is active
	GetWorldTimerManager().ClearTimer(HoverDurationTimer);
	// Start cooldown timer
	GetWorldTimerManager().SetTimer(HoverCooldownTimer, this, &AShooterCharacter::EndHoverCooldown, HoverCooldownAfterMaxDuration, false);

	bHoverOnCooldown = true;
}

void AShooterCharacter::StartAim()
{
	bAiming = true;
}

void AShooterCharacter::StopAim()
{
	bAiming = false;
}

void AShooterCharacter::CameraInterpZoom(float DeltaTime)
{
	// Set current camera field of view
	if (bAiming)
	{
		// Interpolate to zoomed FOV
		CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraZoomedFOV, DeltaTime, ZoomInterpSpeed);
	}
	else
	{
		// Interpolate to default FOV
		CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraDefaultFOV, DeltaTime, ZoomInterpSpeed);
	}
		
	GetCamera()->SetFieldOfView(CameraCurrentFOV);
}

void AShooterCharacter::InventorySlot1()
{
	WeaponComponent->EquipWeapon(PlayerLoadout.PrimaryWeapon, 0, RetargetedMesh, RightHandSocket);
}

void AShooterCharacter::InventorySlot2()
{
	WeaponComponent->EquipWeapon(PlayerLoadout.SecondaryWeapon, 1, RetargetedMesh, RightHandSocket);
}

void AShooterCharacter::InventorySlot3()
{
	WeaponComponent->EquipWeapon(PlayerLoadout.ThirdWeapon, 2, RetargetedMesh, RightHandSocket);
}

void AShooterCharacter::InventorySlot4()
{
	WeaponComponent->EquipWeapon(PlayerLoadout.FourthWeapon, 3, RetargetedMesh, RightHandSocket);
}

bool AShooterCharacter::CanStartHover()
{
	FHitResult Hit;
	FVector TraceStart = GetActorLocation();
	FVector TraceEnd = GetActorLocation() + GetActorUpVector() * -300.f;

	FCollisionQueryParams QParams;
	QParams.AddIgnoredActor(this);
	TEnumAsByte<ECollisionChannel> TraceProps = ECC_Visibility;

	GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, TraceProps, QParams);

	if (GetCharacterMovement()->IsFalling() || GetCharacterMovement()->IsMovingOnGround())  //if (Hit.bBlockingHit == false && GetCharacterMovement()->IsFalling() == true)
	{
		return !bHoverOnCooldown && !bIsHovering;
	}

	return false;
}

void AShooterCharacter::EndHoverCooldown()
{
	bHoverOnCooldown = false;
}

void AShooterCharacter::RecordOriginalSettings()
{
	OriginalTurnRate = GetCharacterMovement()->RotationRate;
	OriginalOrientRotation = GetCharacterMovement()->bOrientRotationToMovement;
	OriginalGravityScale = GetCharacterMovement()->GravityScale;
	OriginalWalkingSpeed = GetCharacterMovement()->MaxWalkSpeed;
	OriginalDeceleration = GetCharacterMovement()->BrakingDecelerationFalling;
	OriginalAcceleration = GetCharacterMovement()->MaxAcceleration;
	OriginalAirControl = GetCharacterMovement()->AirControl;
	OriginalDesiredRotation = GetCharacterMovement()->bUseControllerDesiredRotation;
}

void AShooterCharacter::ApplyOriginalSettings()
{
	GetCharacterMovement()->RotationRate = OriginalTurnRate;
	GetCharacterMovement()->bOrientRotationToMovement = OriginalOrientRotation;
	GetCharacterMovement()->GravityScale = OriginalGravityScale;
	GetCharacterMovement()->MaxWalkSpeed = OriginalWalkingSpeed;
	GetCharacterMovement()->BrakingDecelerationFalling = OriginalDeceleration;
	GetCharacterMovement()->MaxAcceleration = OriginalAcceleration;
	GetCharacterMovement()->AirControl = OriginalAirControl;
	GetCharacterMovement()->bUseControllerDesiredRotation = OriginalDesiredRotation;
}

void AShooterCharacter::DescentPlayer(float DeltaTime)
{
	if (CurrentVelocity.Z != DescendingRate * -1.f && bIsHovering == true) 
	{
		CurrentVelocity.Z = UKismetMathLibrary::FInterpTo(CurrentVelocity.Z, DescendingRate, DeltaTime, 2.f);
		GetCharacterMovement()->Velocity.Z = CurrentVelocity.Z;
	}
}

void AShooterCharacter::Crouch()
{
	if (!GetMovementComponent()->IsMovingOnGround())
	{
		return;
	}

	!bIsCrouched ? ACharacter::Crouch() : ACharacter::UnCrouch();
}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Add Input Mapping Context
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Movement
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);

		// Look
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);

		// Sprint
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ThisClass::Sprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ThisClass::StopSprinting);

		// Jump 
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Fire
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &ThisClass::Fire);

		// Interact
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &ThisClass::Interact);

		// Hover
		EnhancedInputComponent->BindAction(HoverAction, ETriggerEvent::Triggered, this, &ThisClass::StartHover);
		EnhancedInputComponent->BindAction(HoverAction, ETriggerEvent::Completed, this, &ThisClass::StopHover);

		// Aim
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Triggered, this, &ThisClass::StartAim);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &ThisClass::StopAim);

		// Crouch/Un-crouch
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &ThisClass::Crouch);

		// Inventory
		EnhancedInputComponent->BindAction(InventorySlot1Action, ETriggerEvent::Triggered, this, &ThisClass::InventorySlot1);
		EnhancedInputComponent->BindAction(InventorySlot2Action, ETriggerEvent::Triggered, this, &ThisClass::InventorySlot2);
		EnhancedInputComponent->BindAction(InventorySlot3Action, ETriggerEvent::Triggered, this, &ThisClass::InventorySlot3);
		EnhancedInputComponent->BindAction(InventorySlot4Action, ETriggerEvent::Triggered, this, &ThisClass::InventorySlot4);
	}
}