// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/NMPlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/Components/NMWeaponComponent.h"
#include "InventorySystem/Weapons/NMWeaponActor.h"
#include "Net/UnrealNetwork.h"
#include "UI/NMHUDBase.h"

ANMPlayerCharacter::ANMPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MaxWalkSpeedSprinting(680.f)
	, CameraDefaultFOV(0.f)
	, CameraCurrentFOV(0.f)
	, CameraZoomedFOV(65.f)
	, ZoomInterpSpeed(20.f)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	
	// Mesh that will be retarget from Manny
	RetargetedMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RetargetedMesh"));
	RetargetedMesh->SetupAttachment(GetMesh());

	// Follow rotation only on Yaw. 
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;
}

void ANMPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (GetCamera())
	{
		// Cache default FOV
		CameraDefaultFOV = GetCamera()->FieldOfView;
		CameraCurrentFOV = CameraDefaultFOV;
	}

	// Cache the default walk speed
	GetCharacterMovement()->MaxCustomMovementSpeed = GetCharacterMovement()->MaxWalkSpeed;
}

void ANMPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Spawn the AR weapon and equip it to the mesh
	if (HasAuthority())
	{
		InventorySlot1();
	}
}

void ANMPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle interpolation for zoom when aiming
	CameraInterpZoom(DeltaTime);
}

void ANMPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Triggered, this, &ThisClass::StartAim);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &ThisClass::StopAim);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ThisClass::Sprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ThisClass::StopSprinting);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &ThisClass::Fire);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &ThisClass::StopFire);
		EnhancedInputComponent->BindAction(ThrowGrenade, ETriggerEvent::Started, this, &ThisClass::Throw);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ThisClass::Crouch);
		EnhancedInputComponent->BindAction(InventorySlot1Action, ETriggerEvent::Started, this, &ThisClass::InventorySlot1);
		EnhancedInputComponent->BindAction(InventorySlot2Action, ETriggerEvent::Started, this, &ThisClass::InventorySlot2);
		EnhancedInputComponent->BindAction(InventorySlot3Action, ETriggerEvent::Started, this, &ThisClass::InventorySlot3);
		EnhancedInputComponent->BindAction(InventorySlot4Action, ETriggerEvent::Started, this, &ThisClass::InventorySlot4);
		EnhancedInputComponent->BindAction(InventorySlot5Action, ETriggerEvent::Started, this, &ThisClass::InventorySlot5);
		EnhancedInputComponent->BindAction(MainMenuAction, ETriggerEvent::Started, this, &ThisClass::MainMenu);
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &ThisClass::Reload);
	}
}

void ANMPlayerCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ANMPlayerCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ANMPlayerCharacter::StartAim()
{
	if (!GameplaytoTag.GameplayTag_IsADS)
	{
		GameplaytoTag.GameplayTag_IsADS = true;
		OnGameplayTagChanged.Broadcast(GameplaytoTag);
	}
}

void ANMPlayerCharacter::StopAim()
{
	GameplaytoTag.GameplayTag_IsADS = false;
	OnGameplayTagChanged.Broadcast(GameplaytoTag);
}

void ANMPlayerCharacter::CameraInterpZoom(float DeltaTime)
{
	float TargetFOV = GameplaytoTag.GameplayTag_IsADS ? CameraZoomedFOV : CameraDefaultFOV;
	// Set current camera field of view
	CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, TargetFOV, DeltaTime, ZoomInterpSpeed);

	GetCamera()->SetFieldOfView(CameraCurrentFOV);
}

void ANMPlayerCharacter::Sprint()
{
	if (!HasAuthority())
	{
		ServerSprint();
	}
	
	GetCharacterMovement()->MaxWalkSpeed = MaxWalkSpeedSprinting;
}

void ANMPlayerCharacter::StopSprinting()
{
	if (!HasAuthority())
	{
		ServerStopSprinting();
	}

	GetCharacterMovement()->MaxWalkSpeed = GetCharacterMovement()->MaxCustomMovementSpeed;
}

void ANMPlayerCharacter::ServerSprint_Implementation()
{
	Sprint();
}

void ANMPlayerCharacter::ServerStopSprinting_Implementation()
{
	StopSprinting();
}

void ANMPlayerCharacter::Fire()
{
	GameplaytoTag.GameplayTag_IsFiring = !GameplaytoTag.GameplayTag_IsFiring;
	OnGameplayTagChanged.Broadcast(GameplaytoTag);
	WeaponComponent->FireWeapon();
}

void ANMPlayerCharacter::StopFire()
{
	WeaponComponent->StopFireWeapon();
}

void ANMPlayerCharacter::Throw()
{
	WeaponComponent->ThrowGrenade();
}

void ANMPlayerCharacter::Reload()
{
	GameplaytoTag.GameplayTag_IsReloading = !GameplaytoTag.GameplayTag_IsReloading;
	OnGameplayTagChanged.Broadcast(GameplaytoTag);
	WeaponComponent->ReloadWeapon();
}

void ANMPlayerCharacter::Crouch()
{
	if (!GetMovementComponent()->IsMovingOnGround()) return;

	GameplaytoTag.GameplayTag_IsCrouching = !GameplaytoTag.GameplayTag_IsCrouching;
	GameplaytoTag.GameplayTag_IsCrouching ? ACharacter::Crouch() : ACharacter::UnCrouch();
}

void ANMPlayerCharacter::InventorySlot1()
{
	WeaponComponent->EquipWeapon(PlayerLoadout.PrimaryWeapon, 0, RetargetedMesh, RightHandSocket);
}

void ANMPlayerCharacter::InventorySlot2()
{
	WeaponComponent->EquipWeapon(PlayerLoadout.SecondaryWeapon, 1, RetargetedMesh, RightHandSocket);
}

void ANMPlayerCharacter::InventorySlot3()
{
	WeaponComponent->EquipWeapon(PlayerLoadout.ThirdWeapon, 2, RetargetedMesh, RightHandSocket);
}

void ANMPlayerCharacter::InventorySlot4()
{
	WeaponComponent->EquipWeapon(PlayerLoadout.FourthWeapon, 3, RetargetedMesh, RightHandSocket);
}

void ANMPlayerCharacter::InventorySlot5()
{
	WeaponComponent->EquipWeapon(PlayerLoadout.FifthWeapon, 4, RetargetedMesh, RightHandSocket);
}

void ANMPlayerCharacter::MainMenu()
{
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (ANMHUDBase* HUD = PlayerController->GetHUD<ANMHUDBase>())
		{
			HUD->ToggleMainMenu();
		}
	}
}