
#include "Main/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Main/MainPlayerController.h"
#include "Interfaces/AnimDataReceiverInterface.h"
#include "Interfaces/PickupInterface.h"
#include "Component/Inventory.h"
#include "Pickups/PickupBase.h"
#include "Pickups/WeaponBaseActor.h"
#include "Engine/EngineTypes.h"
#include "DrawDebugHelpers.h"
#include "Component/GrenadeSystem.h"


APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	InHandGrenadeSocketName = FName("InHandGrenadeSocket");
	InHandEnergyDrinkSocketName = FName("InHandEnergyDrinkSocket");

	CurrentLocomotionState = ELocomotionState::Unarmed;
	bCanShoot = true;
	bPrimaryWeaponStored = false;
	bEnemyDetected = false;
	bPlayCrosshairSpreadAnim = false;
	SwitchingWeaponTimes_Rifle = 0;
	SwitchingWeaponTimes_Pistol = 0;

	PlayerCapsule = GetCapsuleComponent();
	PlayerCapsule->InitCapsuleSize(42.0f, 92.0f);

	InteractionArea = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	InteractionArea->SetupAttachment(PlayerCapsule);
	InteractionArea->InitSphereRadius(80.0f);
	InteractionArea->SetCollisionProfileName(FName("OverlapAllDynamic"), true);

	PlayerMesh = GetMesh();
	PlayerMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -95.0f));
	PlayerMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	PlayerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	InHandMesh->SetupAttachment(PlayerMesh, InHandGrenadeSocketName);
	InHandMesh->SetVisibility(false);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->SetupAttachment(PlayerCapsule);
	SpringArm->TargetArmLength = 300.0f;
	SpringArm->SetRelativeLocation(FVector(0.0f, 60.0f, 10.0f));
	SpringArm->SocketOffset = FVector(0.0f, 60.0f, 40.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	GrenadeThrowTransform = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	GrenadeThrowTransform->SetupAttachment(Camera);
	GrenadeThrowTransform->SetRelativeLocation(FVector(355.0f, -105.0f, -5.0f));
	GrenadeThrowTransform->SetRelativeRotation(FRotator(0.0f, 0.0f, 30.0f));

	PlayerInventory = CreateDefaultSubobject<UInventory>(TEXT("ActorComponent"));

	GrenadeSystem = CreateDefaultSubobject<UGrenadeSystem>(TEXT("GrenadeComponent"));

	PlayerMovement = GetCharacterMovement();
	PlayerMovement->bOrientRotationToMovement = false;
	PlayerMovement->bUseControllerDesiredRotation = false;
	PlayerMovement->NavAgentProps.bCanCrouch = true;
	PlayerMovement->JumpZVelocity = 700.0f;
	PlayerMovement->BrakingDecelerationFalling = 1500.0f;
	PlayerMovement->AirControl = 0.35f;

	GaitSettingMap.Add(EGait::Walking, { 250.0f,250.0f,250.0f,1.0f,0.0f,true });
	GaitSettingMap.Add(EGait::Jogging, { 800.0f,800.0f,1200.0f,1.0f,0.0f,true });
	GaitSettingMap.Add(EGait::Crouching, { 250.0f,250.0f,250.0f,1.0f,0.0f,true });
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	PlayerController = Cast<AMainPlayerController>(GetController());
	if (PlayerController)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	PlayerAnimInstance = PlayerMesh->GetAnimInstance();
	UpdateGaitSettings(EGait::Jogging);
	UpdateLocomotionState(ELocomotionState::Unarmed);

	InteractionArea->OnComponentBeginOverlap.AddDynamic(this, &APlayerCharacter::OnInteractionAreaBeginOverlap);
	InteractionArea->OnComponentEndOverlap.AddDynamic(this, &APlayerCharacter::OnInteractionAreaEndOverlap);
	PlayerAnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &APlayerCharacter::OnMontageNotifyBegin);

	// UpdateFieldOfViewTimeline
	FOnTimelineFloat ProgressFieldOfViewFunction;
	ProgressFieldOfViewFunction.BindUFunction(this, FName("UpdateFieldOfView"));
	UpdateFieldOfViewTimeline.AddInterpFloat(UpdateFieldOfViewCurve, ProgressFieldOfViewFunction);
	UpdateFieldOfViewTimeline.SetLooping(false);
	UpdateFieldOfViewTimeline.SetPlayRate(1.0f);

	//UpdateSpringArmOnCrouchTimeline
	FOnTimelineFloat UpdateSpringArmOnCrouch;
	UpdateSpringArmOnCrouch.BindUFunction(this, FName("UpdateSpringArmOnCrouch"));
	UpdateSpringArmOnCrouchTimeline.AddInterpFloat(UpdateFieldOfViewCurve, UpdateSpringArmOnCrouch);
	UpdateSpringArmOnCrouchTimeline.SetLooping(false);
	UpdateSpringArmOnCrouchTimeline.SetPlayRate(0.5f);

	//TraceForPickups
	FTimerHandle PickupTraceTimer;
	GetWorld()->GetTimerManager().SetTimer(
		PickupTraceTimer,
		this,
		&APlayerCharacter::TraceForPickups,
		0.05f, // Trace frequency
		true
	);
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateFieldOfViewTimeline.TickTimeline(DeltaTime);
	UpdateSpringArmOnCrouchTimeline.TickTimeline(DeltaTime);
	CrosshairSpreadSetup();
	CheckIfEnemyInFiringRange();
	if (GrenadeSystem)
	{
		GrenadeSystem->DrawPredictionSpline();
	}
}

void APlayerCharacter::UpdateGaitSettings(EGait Gait)
{
	const FGaitSetting* FoundGaitSettings = GaitSettingMap.Find(Gait);
	CurrentGait = Gait;
	IAnimDataReceiverInterface::Execute_ReceiveCurrentGait(PlayerAnimInstance, CurrentGait);
	PlayerMovement->MaxWalkSpeed = FoundGaitSettings->MaxWalkSpeed;
	PlayerMovement->MaxAcceleration = FoundGaitSettings->MaxAcceleration;
	PlayerMovement->BrakingDecelerationWalking = FoundGaitSettings->BrakingDeceleration;
	PlayerMovement->BrakingFrictionFactor = FoundGaitSettings->BrakingFrictionFactor;
	PlayerMovement->BrakingFriction = FoundGaitSettings->BrakingFriction;
	PlayerMovement->bUseSeparateBrakingFriction = FoundGaitSettings->bUseSeparateBrakingFriction;
}

void APlayerCharacter::UpdateLocomotionState(ELocomotionState State)
{
	CurrentLocomotionState = State;
	IAnimDataReceiverInterface::Execute_ReceiveCurrentLocomotionState(PlayerAnimInstance, CurrentLocomotionState);
	switch (State)
	{
	case ELocomotionState::Unarmed:
		PlayerAnimInstance->LinkAnimClassLayers(UnarmedAnimLayer);
		break;
	case ELocomotionState::Rifle:
		PlayerAnimInstance->LinkAnimClassLayers(RifleAnimLayer);
		break;
	case ELocomotionState::Pistol:
		PlayerAnimInstance->LinkAnimClassLayers(PistolAnimLayer);
		break;
	case ELocomotionState::Shotgun:
		PlayerAnimInstance->LinkAnimClassLayers(ShotgunAnimLayer);
		break;
	default:
		break;
	}
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	FVector2d MovementVector = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), MovementVector.Y);
	AddMovementInput(GetActorRightVector(), MovementVector.X);
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void APlayerCharacter::StartWalk()
{
	UpdateGaitSettings(EGait::Walking);
}

void APlayerCharacter::StopWalk()
{
	UpdateGaitSettings(EGait::Jogging);
}

void APlayerCharacter::CustomCrouch()
{
	if (bCrouched)
	{
		bCrouched = false;
		UpdateGaitSettings(EGait::Jogging);
	}
	else
	{
		bCrouched = true;
		UpdateGaitSettings(EGait::Crouching);
	}
}

void APlayerCharacter::StartInteract()
{
	if (!FocusedPickupActor) return;
	if (FocusedPickupActor->Implements<UPickupInterface>())
	{
		PushPickupInteractNotification(EPickupInteractNotificationType::Start, FocusedPickupActor->PickupData);
		GetWorld()->GetTimerManager().SetTimer(InteractTimerHandle, [this]()
			{
				Interact();
			}, FocusedPickupActor->PickupData.InteractionTime, false);
	}
}

void APlayerCharacter::StopInteract()
{
	if (!FocusedPickupActor) return;
	if (FocusedPickupActor->Implements<UPickupInterface>())
	{
		if (InteractTimerHandle.IsValid())
		{
			PushPickupInteractNotification(EPickupInteractNotificationType::Cancel, FocusedPickupActor->PickupData);
			GetWorld()->GetTimerManager().ClearTimer(InteractTimerHandle);
			InteractTimerHandle.Invalidate();
		}
	}
}

void APlayerCharacter::Interact()
{
	if (FocusedPickupActor)
	{
		if (FocusedPickupActor->Implements<UPickupInterface>())
		{
			PushPickupInteractNotification(EPickupInteractNotificationType::Remove, FocusedPickupActor->PickupData);
			IPickupInterface::Execute_InteractedToPickup(FocusedPickupActor, this);
		}
	}
}

void APlayerCharacter::StartShoot()
{
	if (GrenadeSystem->bInHand)
	{
		ThrowGrenade();
	}
	if (AWeaponBaseActor* EquippedWeapon = GetPrimaryWeapon())
	{
		float FireRate = EquippedWeapon->WeaponData.FireRate;
		Shoot();
		GetWorld()->GetTimerManager().SetTimer(ShootTimerHandle, this, &APlayerCharacter::Shoot, FireRate, true, FireRate);
	}
}

void APlayerCharacter::StopShoot()
{
	if (ShootTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(ShootTimerHandle);
		ShootTimerHandle.Invalidate();
	}
}

void APlayerCharacter::Shoot()
{
	if (bCanShoot && GetPrimaryWeapon())
	{
		AWeaponBaseActor* EquippedWeapon = GetPrimaryWeapon();
		PlayShootMontage();
		EquippedWeapon->Shoot(this, bAdsButtonPressed);
	}
}

void APlayerCharacter::Reloading()
{
	if (AWeaponBaseActor* EquippedWeapon = GetPrimaryWeapon())
	{
		if (EquippedWeapon->CheckAmmo())
		{
			StopShoot();
			bCanShoot = false;
			PlayReloadMontage();
			FTimerHandle ReloadTImerHandle;
			GetWorld()->GetTimerManager().SetTimer(ReloadTImerHandle, [this,EquippedWeapon]()
				{
					if (EquippedWeapon)
					{
						EquippedWeapon->ReloadingWeapon();
						bCanShoot = true;
					}
				}, 3.0f, false);
		}
	}
}

void APlayerCharacter::SetInHandMesh(bool bShow, EPickupType Type)
{
	InHandMesh->SetVisibility(bShow);
	switch (Type)
	{
	case EPickupType::Grenade:
		InHandMesh->SetStaticMesh(GrenadeMesh);
		InHandMesh->AttachToComponent(PlayerMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, InHandGrenadeSocketName);
		break;
	case EPickupType::HealthKit:
		InHandMesh->SetStaticMesh(EnergyDrinkMesh);
		InHandMesh->AttachToComponent(PlayerMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, InHandEnergyDrinkSocketName);
		break;
	default:
		break;
	}
}

void APlayerCharacter::ApplyBandage()
{
	if (PlayerInventory->RemainingBandage > 0)
	{
		EquipUnequip();
		FTimerHandle UnequipTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(UnequipTimerHandle, [this]()
			{
				PlayerAnimInstance->Montage_Play(ApplyBandageMontage);
				LoadingNotification(ELoadingNotificationType::ApplyBandage);
			}, 1.0f, false);
	}
	else
	{
		PushEmptyNotification(EEmptyNotificationType::Bandage);
	}
}

void APlayerCharacter::UseEnergyDrink()
{
	if (PlayerInventory->RemainingEnergyDrink > 0)
	{
		EquipUnequip();
		FTimerHandle UnequipTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(UnequipTimerHandle, [this]()
			{
				SetInHandMesh(true, EPickupType::HealthKit);
				PlayerAnimInstance->Montage_Play(UseEnergyDrinkMontage);
				LoadingNotification(ELoadingNotificationType::UseEnergyDrink);
			}, 1.0f, false);
	}
	else
	{
		PushEmptyNotification(EEmptyNotificationType::EnergyDrink);
	}
}

void APlayerCharacter::Switch()
{
	bSwitchingWeapon = true;
	UnequipWeapon();
	FTimerHandle SwitchTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(SwitchTimerHandle, [this]()
		{
			EquipWeapon();
		}, 1.0f, false);	
}

void APlayerCharacter::EquipUnequip()
{
	if (WeaponSocketController.bPrimaryWeaponSocket)
	{
		UnequipWeapon();
	}
	else
	{
		EquipWeapon();
	}
}

AWeaponBaseActor* APlayerCharacter::GetPrimaryWeapon()
{
	switch (CurrentWeaponSlot)
	{
	case EWeaponSlot::Primary:
		if (PrimaryWeapon) return PrimaryWeapon;
		return nullptr;
		break;
	case EWeaponSlot::Secondary:
		if (SecondaryWeapon) return SecondaryWeapon;
		return nullptr;
		break;
	default:
		return nullptr;
	}
}

AWeaponBaseActor* APlayerCharacter::GetSecondaryWeapon()
{
	switch (CurrentWeaponSlot)
	{
	case EWeaponSlot::Primary:
		if (SecondaryWeapon) return SecondaryWeapon;
		return nullptr;
		break;
	case EWeaponSlot::Secondary:
		if (PrimaryWeapon) return PrimaryWeapon;
		return nullptr;
		break;
	default: return nullptr;
	}
}

void APlayerCharacter::UpdateEquippedWeapon(AWeaponBaseActor* Weapon)
{
	switch (CurrentWeaponSlot)
	{
	case EWeaponSlot::Primary:
		PrimaryWeapon = Weapon;
		break;
	case EWeaponSlot::Secondary:
		SecondaryWeapon = Weapon;
		break;
	}
}

//NeedCustomization
void APlayerCharacter::UpdateWeaponAttachment()
{
	if (AWeaponBaseActor* EquippedWeapon = GetPrimaryWeapon())
	{
		switch (EquippedWeapon->WeaponData.Type)
		{
		case EWeaponType::Rifle:
			UpdateCrossHair(true, EWeaponType::Rifle);
			break;
		case EWeaponType::Pistol:
			UpdateCrossHair(true, EWeaponType::Pistol);
			break;
		case EWeaponType::Shotgun:
			UpdateCrossHair(true, EWeaponType::Shotgun);
			break;
		}
		EquippedWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), EquippedWeapon->WeaponData.PrimaryWeaponSocketName);
		WeaponSocketController.bPrimaryWeaponSocket = true;
		CurrentLocomotionState = EquippedWeapon->WeaponData.PlayerLocomotionType;
		UpdateLocomotionState(CurrentLocomotionState);
	}
	if (AWeaponBaseActor* StoredWeapon = GetSecondaryWeapon())
	{
		switch (StoredWeapon->WeaponData.Type)
		{
		case EWeaponType::Rifle:
		case EWeaponType::Shotgun:
			StoredWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), StoredWeapon->WeaponData.SecondaryWeaponSocketName);
			WeaponSocketController.bSecondaryWeaponStoredSocket_Rifle = true;
			break;
		case EWeaponType::Pistol:
			if (GetPrimaryWeapon()->WeaponData.Type == EWeaponType::Pistol)
			{
				StoredWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), StoredWeapon->WeaponData.SecondaryWeaponSocketName);
				WeaponSocketController.bSecondaryWeaponStoredSocket_Pistol = true;
			}
			else
			{
				StoredWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), StoredWeapon->WeaponData.PrimaryWeaponStoredSocketName);
				WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol = true;
			}
			break;
		default:
			break;
		}
		
	}
}

void APlayerCharacter::PickupWeapon(AWeaponBaseActor* Weapon)
{
	if (PrimaryWeapon)
	{
		if (SecondaryWeapon)
		{
			if (AWeaponBaseActor* EquippedWeapon = GetPrimaryWeapon())
			{
				EquippedWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				EquippedWeapon->Drop();
				EquippedWeapon->ApplyForwardImpulse(GetActorForwardVector());
				UpdateEquippedWeapon(nullptr);
				PickupWeapon(Weapon);
			}
		}
		else
		{
			SecondaryWeapon = Weapon;
			UpdateWeaponAttachment();
		}
	}
	else
	{
		PrimaryWeapon = Weapon;
		UpdateWeaponAttachment();
	}
}

UCameraComponent* APlayerCharacter::GetPlayerCamera()
{
	return Camera;
}

void APlayerCharacter::PlayCameraShake()
{
	PlayerController->ClientStartCameraShake(ShootCameraShake);
}

void APlayerCharacter::TraceForPickups()
{
	FVector CameraLocation;
	FRotator CameraRotation;
	GetController()->GetPlayerViewPoint(CameraLocation, CameraRotation);

	FVector TraceStart = CameraLocation;
	FVector TraceEnd = TraceStart + (CameraRotation.Vector() * 750.0f);

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(this);

	FHitResult HitResult;

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	if (bHit && HitResult.GetActor()->Implements<UPickupInterface>())
	{
		FocusedPickupActor = Cast<APickupBase>(HitResult.GetActor());
		IPickupInterface::Execute_InteractionAreaEntered(FocusedPickupActor, this);
		PushPickupInteractNotification(EPickupInteractNotificationType::Add, FocusedPickupActor->PickupData);
	}
	else if (FocusedPickupActor)
	{
		StopInteract();
		PushPickupInteractNotification(EPickupInteractNotificationType::Remove, FocusedPickupActor->PickupData);
		IPickupInterface::Execute_InteractionAreaExited(FocusedPickupActor);
		FocusedPickupActor = nullptr;
	}
}

//NeedCustomization
void APlayerCharacter::StorePrimaryWeapon()
{
	if (AWeaponBaseActor* Weapon = GetPrimaryWeapon())
	{
		PrimaryWeaponStoredSocket = Weapon->WeaponData.PrimaryWeaponStoredSocketName;
		Weapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), PrimaryWeaponStoredSocket);
		CurrentLocomotionState = ELocomotionState::Unarmed;
		UpdateLocomotionState(CurrentLocomotionState);
		UpdateCrossHair(false, Weapon->WeaponData.Type);
	}
}

void APlayerCharacter::PlayReloadMontage()
{
	if (AWeaponBaseActor* Weapon = GetPrimaryWeapon())
	{
		LoadingNotification(ELoadingNotificationType::Reloading);
		switch (Weapon->WeaponData.Type)
		{
		case EWeaponType::Rifle:
			PlayAnimMontage(ReloadRifleMontage);
			break;
		case EWeaponType::Pistol:
			PlayAnimMontage(ReloadPistolMontage);
			break;
		case EWeaponType::Shotgun:
			PlayAnimMontage(ReloadShotgunMontage);
			break;
		}
	}
}

void APlayerCharacter::PlayShootMontage()
{
	if (AWeaponBaseActor* Weapon = GetPrimaryWeapon())
	{
		switch (Weapon->WeaponData.Type)
		{
		case EWeaponType::Rifle:
			PlayAnimMontage(ShootRifleMontage);
			break;
		case EWeaponType::Pistol:
			PlayAnimMontage(ShootPistolMontage);
			break;
		case EWeaponType::Shotgun:
			PlayAnimMontage(ShootShotgunMontage);
			break;
		}
	}
}

void APlayerCharacter::CheckIfEnemyInFiringRange()
{
	if (!GetPrimaryWeapon()) return;
	float Range = GetPrimaryWeapon()->WeaponData.FiringRange;
	FVector StartLocation = Camera->GetComponentLocation();
	FVector EndLocation = StartLocation + (Camera->GetForwardVector() * Range);
	FHitResult HitResult;

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECollisionChannel::ECC_Visibility);
	if (bHit)
	{
		if (HitResult.GetActor()->ActorHasTag(FName("Enemy")))
		{
			if (!bEnemyDetected)
			{
				bEnemyDetected = true;
				PlayEnemyDetectedCrosshairAnim(bEnemyDetected);
			}
		}
		else
		{
			if (bEnemyDetected)
			{
				bEnemyDetected = false;
				PlayEnemyDetectedCrosshairAnim(bEnemyDetected);
			}
		}
	}
	else
	{
		if (bEnemyDetected)
		{
			bEnemyDetected = false;
			PlayEnemyDetectedCrosshairAnim(bEnemyDetected);
		}
	}
}

void APlayerCharacter::CrosshairSpreadSetup()
{
	if (PlayerMovement->GetCurrentAcceleration().Size2D() > 10.0f)
	{
		if (!bPlayCrosshairSpreadAnim)
		{
			bPlayCrosshairSpreadAnim = true;
			PlayCrosshairSpreadAnim(bPlayCrosshairSpreadAnim);
		}
	}
	else
	{
		if (bPlayCrosshairSpreadAnim)
		{
			bPlayCrosshairSpreadAnim = false;
			PlayCrosshairSpreadAnim(bPlayCrosshairSpreadAnim);
		}
	}
}

void APlayerCharacter::UnequipWeapon()
{
	if (AWeaponBaseActor* EquippedWeapon = GetPrimaryWeapon())
	{
		CurrentEquippedWeapon = GetPrimaryWeapon();
		switch (EquippedWeapon->WeaponData.Type)
		{
		case EWeaponType::Rifle:
		case EWeaponType::Shotgun:
			if (WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle)
			{
				PlayUnequipWeaponMontage(EWeaponType::Rifle, EWeaponSlot::Secondary);
				break;
			}
			else
			{
				PlayUnequipWeaponMontage(EWeaponType::Rifle, EWeaponSlot::Primary);
				break;
			}
		case EWeaponType::Pistol:
			if (WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol)
			{
				PlayUnequipWeaponMontage(EWeaponType::Pistol, EWeaponSlot::Secondary);
				break;
			}
			else
			{
				PlayUnequipWeaponMontage(EWeaponType::Pistol, EWeaponSlot::Primary);
				break;
			}
		default:
			break;
		}
	}
}


void APlayerCharacter::EquipWeapon()
{
	if (bSwitchingWeapon)
	{
		if (!GetSecondaryWeapon()) return;
		CurrentEquippedWeapon = GetSecondaryWeapon();

		switch (CurrentWeaponSlot)
		{
		case EWeaponSlot::Primary:
			CurrentWeaponSlot = EWeaponSlot::Secondary;
			break;
		case EWeaponSlot::Secondary:
			CurrentWeaponSlot = EWeaponSlot::Primary;
			break;
		}
	}
	else
	{
		if (!GetPrimaryWeapon()) return;
		CurrentEquippedWeapon = GetPrimaryWeapon();
	}
	UpdateEquippedWeapon(CurrentEquippedWeapon);
	if (GetPrimaryWeapon())
	{
		switch (CurrentEquippedWeapon->WeaponData.Type)
		{
		case EWeaponType::Rifle:
		case EWeaponType::Shotgun:
			if (WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle)
			{
				PlayEquipWeaponMontage(EWeaponType::Rifle, EWeaponSlot::Primary);
				break;
			}
			else
			{
				PlayEquipWeaponMontage(EWeaponType::Rifle, EWeaponSlot::Secondary);
				break;
			}
		case EWeaponType::Pistol:
			if (WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol)
			{
				PlayEquipWeaponMontage(EWeaponType::Pistol, EWeaponSlot::Primary);
				break;
			}
			else
			{
				PlayEquipWeaponMontage(EWeaponType::Pistol, EWeaponSlot::Secondary);
				break;
			}
		default:
			break;
		}
	}
}

void APlayerCharacter::PlayUnequipWeaponMontage(EWeaponType Type, EWeaponSlot Slot)
{
	switch (Type)
	{
	case EWeaponType::Rifle:
	case EWeaponType::Shotgun:
		switch (Slot)
		{
		case EWeaponSlot::Primary:
			PlayerAnimInstance->Montage_Play(UnequipToPrimaryWeaponSocketRifleMontage);
			break;
		case EWeaponSlot::Secondary:
			PlayerAnimInstance->Montage_Play(UnequipToSecondaryWeaponSocketRifleMontage);
			break;
		default:
			break;
		}
		break;
	case EWeaponType::Pistol:
		switch (Slot)
		{
		case EWeaponSlot::Primary:
			PlayerAnimInstance->Montage_Play(UnequipToPrimaryWeaponSocketPistolMontage);
			break;
		case EWeaponSlot::Secondary:
			PlayerAnimInstance->Montage_Play(UnequipToSecondaryWeaponSocketPistolMontage);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

void APlayerCharacter::PlayEquipWeaponMontage(EWeaponType Type, EWeaponSlot Slot)
{
	switch (Type)
	{
	case EWeaponType::Rifle:
	case EWeaponType::Shotgun:
		switch (Slot)
		{
		case EWeaponSlot::Primary:
			PlayerAnimInstance->Montage_Play(EquipFromPrimaryWeaponSocketRifleMontage);
			break;
		case EWeaponSlot::Secondary:
			PlayerAnimInstance->Montage_Play(EquipFromSecondaryWeaponSocketRifleMontage);
			break;
		default:
			break;
		}
		break;
	case EWeaponType::Pistol:
		switch (Slot)
		{
		case EWeaponSlot::Primary:
			PlayerAnimInstance->Montage_Play(EquipFromPrimaryWeaponSocketPistolMontage);
			break;
		case EWeaponSlot::Secondary:
			PlayerAnimInstance->Montage_Play(EquipFromSecondaryWeaponSocketPistolMontage);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

void APlayerCharacter::UpdateWeaponAttachment(bool bEquip)
{
	if (bEquip)
	{
		CurrentEquippedWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), CurrentEquippedWeapon->WeaponData.TemporaryWeaponSocketName);
		UpdateCrossHair(true, CurrentEquippedWeapon->WeaponData.Type);
		WeaponSocketController.bPrimaryWeaponSocket = true;
		bCanShoot = true;

		switch (CurrentEquippedWeapon->WeaponData.Type)
		{
		case EWeaponType::Rifle:
		case EWeaponType::Shotgun:
			if (WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle)
			{
				if (bSwitchingWeapon)
				{
					if (SwitchingWeaponTimes_Rifle == 0)
					{
						WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle = true;
						WeaponSocketController.bSecondaryWeaponStoredSocket_Rifle = false;
					}
					else if (SwitchingWeaponTimes_Rifle % 2 == 0)
					{
						WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle = true;
						WeaponSocketController.bSecondaryWeaponStoredSocket_Rifle = false;
					}
					else
					{
						WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle = false;
						WeaponSocketController.bSecondaryWeaponStoredSocket_Rifle = true;
					}
					SwitchingWeaponTimes_Rifle = SwitchingWeaponTimes_Rifle + 1;
					bSwitchingWeapon = false;
				}
				else
				{
					switch (CurrentWeaponSlot)
					{
					case EWeaponSlot::Primary:
						WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle = false;
						break;
					case EWeaponSlot::Secondary:
						WeaponSocketController.bSecondaryWeaponStoredSocket_Rifle = false;
						break;
					default:
						WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle = false;
					}
				}	
			}
			else
			{
				WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle = true;
				WeaponSocketController.bSecondaryWeaponStoredSocket_Rifle = false;
			}
			CurrentLocomotionState = ELocomotionState::Rifle;
			UpdateLocomotionState(CurrentLocomotionState);
			break;
		case EWeaponType::Pistol:
			if (WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol)
			{
				if (bSwitchingWeapon)
				{
					if (GetSecondaryWeapon()->WeaponData.Type != EWeaponType::Pistol)
					{
						WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol = false;
					}
					else
					{
						if (SwitchingWeaponTimes_Pistol == 0)
						{
							WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol = true;
							WeaponSocketController.bSecondaryWeaponStoredSocket_Pistol = false;
						}
						else if (SwitchingWeaponTimes_Pistol % 2 == 0)
						{
							WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol = true;
							WeaponSocketController.bSecondaryWeaponStoredSocket_Pistol = false;
						}
						else
						{
							WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol = false;
							WeaponSocketController.bSecondaryWeaponStoredSocket_Pistol = true;
						}
						SwitchingWeaponTimes_Pistol = SwitchingWeaponTimes_Pistol + 1;
					}
					bSwitchingWeapon = false;
				}
				else
				{
					switch (CurrentWeaponSlot)
					{
					case EWeaponSlot::Primary:
						WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol = false;
						break;
					case EWeaponSlot::Secondary:
						WeaponSocketController.bSecondaryWeaponStoredSocket_Pistol = false;
						break;
					default:
						WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol = false;
					}
				}
				
			}
			else
			{
				WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol = true;
				WeaponSocketController.bSecondaryWeaponStoredSocket_Pistol = false;
			}
			CurrentLocomotionState = ELocomotionState::Pistol;
			UpdateLocomotionState(CurrentLocomotionState);
			break;
		default:
			break;
		}
	}
	else
	{
		//Unequipping
		bCanShoot = false;
		CurrentLocomotionState = ELocomotionState::Unarmed;
		UpdateLocomotionState(CurrentLocomotionState);
		UpdateCrossHair(false, CurrentEquippedWeapon->WeaponData.Type);
		WeaponSocketController.bPrimaryWeaponSocket = false;
		switch (CurrentEquippedWeapon->WeaponData.Type)
		{
		case EWeaponType::Rifle:
		case EWeaponType::Shotgun:
			if (GetSecondaryWeapon())
			{
				if (GetSecondaryWeapon()->WeaponData.Type == EWeaponType::Pistol)
				{
					WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle = false;
					WeaponSocketController.bSecondaryWeaponStoredSocket_Rifle = true;
					CurrentEquippedWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), CurrentEquippedWeapon->WeaponData.SecondaryWeaponSocketName);
				}
				else
				{
					if (WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle)
					{
						CurrentEquippedWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), CurrentEquippedWeapon->WeaponData.SecondaryWeaponSocketName);
						WeaponSocketController.bSecondaryWeaponStoredSocket_Rifle = true;
					}
					else
					{
						CurrentEquippedWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), CurrentEquippedWeapon->WeaponData.PrimaryWeaponStoredSocketName);
						WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle = true;
					}
				}
			}
			else
			{
				if (WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle)
				{
					CurrentEquippedWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), CurrentEquippedWeapon->WeaponData.SecondaryWeaponSocketName);
					WeaponSocketController.bSecondaryWeaponStoredSocket_Rifle = true;
				}
				else
				{
					CurrentEquippedWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), CurrentEquippedWeapon->WeaponData.PrimaryWeaponStoredSocketName);
					WeaponSocketController.bPrimaryWeaponStoredSocket_Rifle = true;
				}
			}
			break;
		case EWeaponType::Pistol:
			if (WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol)
			{
				CurrentEquippedWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), CurrentEquippedWeapon->WeaponData.SecondaryWeaponSocketName);
				WeaponSocketController.bSecondaryWeaponStoredSocket_Pistol = true;
			}
			else
			{
				CurrentEquippedWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), CurrentEquippedWeapon->WeaponData.PrimaryWeaponStoredSocketName);
				WeaponSocketController.bPrimaryWeaponStoredSocket_Pistol = true;
			}
			break;
		default:
			break;

		}
	}
}

void APlayerCharacter::StartGrenadeLogic()
{
	
	if (GrenadeSystem->bInHand) return;
	if (PlayerInventory->RemainingGrenade > 0)
	{
		if (WeaponSocketController.bPrimaryWeaponSocket)
		{
			UnequipWeapon();
			FTimerHandle GrenadeLogicTimerHandle;
			GetWorld()->GetTimerManager().SetTimer(GrenadeLogicTimerHandle, [this]()
				{
					PlayerAnimInstance->Montage_Play(GrenadeStartMontage);
					UpdateGaitSettings(EGait::Walking);
					IAnimDataReceiverInterface::Execute_ReceiveCurrentOverlayStates(PlayerAnimInstance, EOverlayStates::Grenade);
					GrenadeSystem->CreatePredictionSpline();
					GrenadeSystem->SetGrenadeMesh(true);

				}, 1.0f, false);
		}
		else
		{
			PlayerInventory->UseGrenade();
			PlayerAnimInstance->Montage_Play(GrenadeStartMontage);
			UpdateGaitSettings(EGait::Walking);
			IAnimDataReceiverInterface::Execute_ReceiveCurrentOverlayStates(PlayerAnimInstance, EOverlayStates::Grenade);
			GrenadeSystem->CreatePredictionSpline();
			GrenadeSystem->SetGrenadeMesh(true);
		}

	}
	else
	{
		PushEmptyNotification(EEmptyNotificationType::Grenade);
	}
}

void APlayerCharacter::ThrowGrenade()
{
	GrenadeSystem->Throw();
	PlayerAnimInstance->Montage_Play(GrenadeThrowMontage);
	UpdateGaitSettings(EGait::Jogging);
	IAnimDataReceiverInterface::Execute_ReceiveCurrentOverlayStates(PlayerAnimInstance, EOverlayStates::None);
	FTimerHandle GrenadeLogicTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(GrenadeLogicTimerHandle, [this]()
		{
			EquipWeapon();
		}, 1.0f, false);
}

void APlayerCharacter::CancelGrenade()
{
	PlayerAnimInstance->Montage_Play(GrenadeCancelMontage);
	UpdateGaitSettings(EGait::Jogging);
	IAnimDataReceiverInterface::Execute_ReceiveCurrentOverlayStates(PlayerAnimInstance, EOverlayStates::None);
	GrenadeSystem->ThrowCancel();
}

//NeedCustomization
void APlayerCharacter::AdsButtonPressed()
{
	if (GrenadeSystem->bInHand)
	{
		CancelGrenade();
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponRaiseSound, GetActorLocation());
		UpdateGaitSettings(EGait::Walking);
		bAdsButtonPressed = true;
		UpdateFieldOfViewTimeline.Play();
		PlayCrosshairAimAnim(bAdsButtonPressed);
	}
}

//NeedCustomization
void APlayerCharacter::AdsButtonReleased()
{
	UGameplayStatics::PlaySoundAtLocation(this, WeaponDownSound, GetActorLocation());
	if (CurrentGait == EGait::Crouching)
	{
		UpdateGaitSettings(EGait::Crouching);
	}
	else 
	{ 
		UpdateGaitSettings(EGait::Jogging);
	}
	bAdsButtonPressed = false;
	UpdateFieldOfViewTimeline.Reverse();
	PlayCrosshairAimAnim(bAdsButtonPressed);
}

bool APlayerCharacter::EquipPrimaryWeapon(EWeaponType WeaponType)
{
	if (CurrentEquippedWeapon)
	{

	}
	return false;
}

void APlayerCharacter::OnMontageNotifyBegin(FName MontageNotify, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload)
{
	if (MontageNotify == "SwitchToMainSocket")
	{
		GetPrimaryWeapon()->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), GetPrimaryWeapon()->WeaponData.PrimaryWeaponSocketName);
	}
	else if (MontageNotify == "SwitchToTempSocket")
	{
		GetPrimaryWeapon()->AttachToComponent(PlayerMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), GetPrimaryWeapon()->WeaponData.TemporaryWeaponSocketName);
	}
	else if (MontageNotify == "WeaponEquippedNotify")
	{
		UpdateWeaponAttachment(true);
	}
	else if (MontageNotify == "WeaponUnequippedNotify")
	{
		UpdateWeaponAttachment(false);
	}
	else if (MontageNotify == "BandageAppliedNotify")
	{
		PlayerInventory->UseBandage();
		EquipUnequip();
	}
	else if (MontageNotify == "EnergyDrinkUsedNotify")
	{
		PlayerInventory->UseEnergyDrink();
		EquipUnequip();
	}
	else if (MontageNotify == "StopAllMontagesNotify")
	{
		if (!PlayerAnimInstance->IsAnyMontagePlaying()) return;
		PlayerAnimInstance->StopAllMontages(0.1f);
	}
	else if (MontageNotify == "DisableInhandMeshNotify")
	{
		InHandMesh->SetVisibility(false);
	}
}

void APlayerCharacter::UpdateSpringArmOnCrouch(float Value)
{
	SpringArm->SetRelativeLocation(FVector(0.0f, 60.0f, FMath::Lerp(40.0f, -10.0f, Value)));
}

void APlayerCharacter::UpdateFieldOfView(float Value)
{
	Camera->FieldOfView = FMath::Lerp(90.0f, 45.0f, Value);
}

void APlayerCharacter::OnInteractionAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//if (OtherActor->Implements<UPickupInterface>())
	//{
	//	PickupActor = Cast<APickupBase>(OtherActor);
	//	IPickupInterface::Execute_InteractionAreaEntered(PickupActor, this);
	//}
}

void APlayerCharacter::OnInteractionAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	//if (PickupActor && OtherActor == PickupActor)
	//{
	//	IPickupInterface::Execute_InteractionAreaExited(OtherActor);
	//	PickupActor = nullptr;
	//}
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Started, this, &APlayerCharacter::StartWalk);
		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopWalk);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopJumping);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &APlayerCharacter::CustomCrouch);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APlayerCharacter::StartInteract);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopInteract);
		EnhancedInputComponent->BindAction(AdsAction, ETriggerEvent::Started, this, &APlayerCharacter::AdsButtonPressed);
		EnhancedInputComponent->BindAction(AdsAction, ETriggerEvent::Completed, this, &APlayerCharacter::AdsButtonReleased);
		EnhancedInputComponent->BindAction(SwitchAction, ETriggerEvent::Completed, this, &APlayerCharacter::Switch);
		EnhancedInputComponent->BindAction(EquipUnequipAction, ETriggerEvent::Completed, this, &APlayerCharacter::EquipUnequip);
		EnhancedInputComponent->BindAction(ShootAction, ETriggerEvent::Started, this, &APlayerCharacter::StartShoot);
		EnhancedInputComponent->BindAction(ShootAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopShoot);
		EnhancedInputComponent->BindAction(ReloadingAction, ETriggerEvent::Completed, this, &APlayerCharacter::Reloading);
		EnhancedInputComponent->BindAction(ApplyBandageAction, ETriggerEvent::Completed, this, &APlayerCharacter::ApplyBandage);
		EnhancedInputComponent->BindAction(UseEnergyDrinkAction, ETriggerEvent::Completed, this, &APlayerCharacter::UseEnergyDrink);
		EnhancedInputComponent->BindAction(ThrowGrenadeAction, ETriggerEvent::Started, this, &APlayerCharacter::StartGrenadeLogic);
		EnhancedInputComponent->BindAction(ThrowGrenadeAction, ETriggerEvent::Completed, this, &APlayerCharacter::ThrowGrenade);

	}
}

