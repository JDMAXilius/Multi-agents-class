
#include "Pickups/WeaponBaseActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Main/PlayerCharacter.h"
#include "Component/Inventory.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Camera/CameraComponent.h"
#include "Actors/BulletBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "Actors/BulletShellBase.h"


AWeaponBaseActor::AWeaponBaseActor()
{
	Mesh->SetCollisionProfileName(FName("NoCollision"), false);

	PickupData.Type = EPickupType::Weapon;
	PickupData.Amount = FText::FromString(TEXT("01"));
	PickupData.Name = FText::FromString(TEXT("Weapon"));
	PickupData.Description = FText::FromString(TEXT("This is Weapon Description example"));

	WeaponData.AmmoClipSize = 50;
	WeaponData.AmmoLoaded = 50;
	WeaponData.Damage = 10.0f;
	WeaponData.FireRate = 0.1f;
	WeaponData.FiringRange = 11000.0f;
	WeaponData.MaxBulletSpreadAngle = 3.0f;

	WeaponData.MuzzleSocketName = FName("MuzzleSocket");
	WeaponData.PrimaryWeaponSocketName = FName("PrimaryWeaponSocket_Rifle");
	WeaponData.SecondaryWeaponSocketName = FName("SecondaryWeaponSocket_Rifle");
	WeaponData.PrimaryWeaponStoredSocketName = FName("PrimaryWeaponStoredSocket_Rifle");
	WeaponData.TemporaryWeaponSocketName = FName("TemporaryWeaponSocket_Rifle");
}

void AWeaponBaseActor::BeginPlay()
{
	Super::BeginPlay();
	SetWeaponState(EWeaponState::Dropped);
}

void AWeaponBaseActor::InteractedToPickup_Implementation(APlayerCharacter* Player)
{
	Super::InteractedToPickup_Implementation(Player);
	Player->PushPickupNotification(EPickupType::Weapon, PickupData.Icon);
	PlayerCharacter = Player;
	SetWeaponState(EWeaponState::Equipped);
	PlayerCharacter->PickupWeapon(this);
}

void AWeaponBaseActor::SetWeaponState(EWeaponState State)
{
	CurrentWeaponState = State;
	switch (CurrentWeaponState)
	{
	case EWeaponState::Dropped:
		InteractionArea->SetCollisionProfileName(FName("OverlapAllDynamic"), true);
		Mesh->SetSimulatePhysics(true);
		Mesh->SetCollisionProfileName(FName("DroppedGun"), false);
		// Add damping to gradually stop motion
		Mesh->SetLinearDamping(1.0f);     // How fast it slows down linearly
		Mesh->SetAngularDamping(2.0f);    // How fast it stops rotating

		// Optional: Limit max angular velocity to prevent excessive rolling
		//->SetPhysicsMaxAngularVelocity(100.0f);  // Limits spin speed
		break;
	case EWeaponState::Equipped:
		InteractionArea->SetCollisionProfileName(FName("NoCollision"), false);
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionProfileName(FName("NoCollision"), false);
		Mesh->ResetRelativeTransform();
		break;
	}
}

void AWeaponBaseActor::Drop()
{
	SetWeaponState(EWeaponState::Dropped);
}

void AWeaponBaseActor::Shoot(APlayerCharacter* Player, bool bAdsButtonPressed)
{
	if (WeaponData.AmmoLoaded > 0)
	{
		FVector MuzzleSocketLocation = Mesh->GetSocketLocation(WeaponData.MuzzleSocketName);
		FRotator MuzzleSocketRotation = Mesh->GetSocketRotation(WeaponData.MuzzleSocketName);
		WeaponData.AmmoLoaded = WeaponData.AmmoLoaded - 1;
		UCameraComponent* PlayerCamera = Player->GetPlayerCamera();
		FVector TraceStartLocation = PlayerCamera->GetComponentLocation();
		FVector AimDirection = PlayerCamera->GetForwardVector();
		if (!bAdsButtonPressed)
		{
			FRotator SpreadRot = GetSpreadAngle(AimDirection, WeaponData.MaxBulletSpreadAngle);
			AimDirection = SpreadRot.Vector();
		}
		FVector TraceEndLocation = TraceStartLocation + (AimDirection * WeaponData.FiringRange);
		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStartLocation, TraceEndLocation, ECollisionChannel::ECC_Visibility, QueryParams);
		UGameplayStatics::SpawnEmitterAttached(WeaponData.MuzzleEffect, Mesh, WeaponData.MuzzleSocketName, MuzzleSocketLocation, MuzzleSocketRotation, FVector(0.5f, 0.5f, 0.5f), EAttachLocation::KeepWorldPosition);
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData.FireSound, MuzzleSocketLocation);
		Player->PlayCameraShake();
		FTransform SpawnBulletTransform;
		FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(MuzzleSocketLocation, (bHit) ? HitResult.ImpactPoint : HitResult.TraceEnd);
		SpawnBulletTransform.SetLocation(MuzzleSocketLocation);
		SpawnBulletTransform.SetRotation(LookAtRotation.Quaternion());
		SpawnBulletTransform.SetScale3D(FVector(1.0f, 1.0f, 1.0f));
		ABulletBase* BulletActor = GetWorld()->SpawnActor<ABulletBase>(BulletClass, SpawnBulletTransform);
		BulletActor->AmmoType = WeaponData.AmmoType;
		BulletActor->SetOwner(this);
		SpawnBulletShell(Player);
		HandleImpact(HitResult, WeaponData.Damage);
	}
	else
	{
		Player->PushEmptyNotification(EEmptyNotificationType::Ammo);
		UGameplayStatics::PlaySound2D(this, WeaponData.DryFireSound);
	}
}

bool AWeaponBaseActor::CheckAmmo()
{
	if (WeaponData.AmmoLoaded < WeaponData.AmmoClipSize)
	{
		switch (WeaponData.AmmoType)
		{
		case EAmmoType::Rifle:
			return PlayerCharacter->PlayerInventory->RemainingRifleAmmo > 0;

		case EAmmoType::Pistol:
			return PlayerCharacter->PlayerInventory->RemainingPistolAmmo > 0;

		case EAmmoType::Shotgun:
			return PlayerCharacter->PlayerInventory->RemainingShotgunAmmo > 0;

		default:
			return false;
		}
	}
	return false;
}

void AWeaponBaseActor::ReloadingWeapon()
{
	switch (WeaponData.AmmoType)
	{
	case EAmmoType::Rifle:
		Reload(PlayerCharacter->PlayerInventory->RemainingRifleAmmo);
		break;

	case EAmmoType::Pistol:
		Reload(PlayerCharacter->PlayerInventory->RemainingPistolAmmo);
		break;

	case EAmmoType::Shotgun:
		Reload(PlayerCharacter->PlayerInventory->RemainingShotgunAmmo);
		break;
	}
}

void AWeaponBaseActor::SpawnBulletShell(AActor* Actor)
{
	FTransform ShellTransform = Mesh->GetSocketTransform(ShellSocketName, RTS_World);
	ABulletShellBase* Shell = GetWorld()->SpawnActor<ABulletShellBase>(BulletShellClass, ShellTransform);
	FVector RightImpulse = Actor->GetActorRightVector();
	RightImpulse.X = RightImpulse.X * FMath::RandRange(100.0f, 200.0f);
	RightImpulse.Y = RightImpulse.Y * FMath::RandRange(200.0f, 300.0f);
	RightImpulse.Z = RightImpulse.Z * FMath::RandRange(100.0f, 200.0f);

	Shell->ShellImpulse(Actor->GetVelocity() + RightImpulse);
}

void AWeaponBaseActor::ApplyForwardImpulse(FVector ImpulseDirection)
{
	Mesh->AddImpulse(ImpulseDirection * 700.0f, NAME_None, true);
}

void AWeaponBaseActor::Reload(int& Ammo)
{
	int AmmoDifference = WeaponData.AmmoClipSize - WeaponData.AmmoLoaded;
	if (AmmoDifference <= Ammo)
	{
		WeaponData.AmmoLoaded = WeaponData.AmmoClipSize;
		Ammo = Ammo - AmmoDifference;
	}
	else
	{
		WeaponData.AmmoLoaded = Ammo + WeaponData.AmmoLoaded;
		Ammo = 0;
	}
}

void AWeaponBaseActor::HandleImpact(FHitResult& Hit, float Damage)
{
	if (AActor* HitActor = Hit.GetActor())
	{
		PlayerCharacter->CrosshairAnimation(false);
		UPrimitiveComponent* HitComponent = Hit.GetComponent();
		FVector HitLocation = Hit.Location;
		FRotator HitRotationFromX = UKismetMathLibrary::MakeRotFromX(Hit.Normal);

		if (HitActor->ActorHasTag(FName("Enemy")))
		{
			UGameplayStatics::ApplyDamage(HitActor, Damage, GetInstigatorController(), PlayerCharacter, UDamageType::StaticClass());
			UGameplayStatics::SpawnDecalAttached(
				BloodDecalMaterial,
				FVector(10.0f, 10.0f, 10.0f),
				HitComponent,
				FName("None"),
				HitLocation,
				HitRotationFromX,
				EAttachLocation::KeepWorldPosition,
				6.0f
			);
			UGameplayStatics::SpawnEmitterAtLocation(
				this,
				BloodParticleSystem,
				HitLocation,
				HitRotationFromX,
				true
			);

		}
		else if (HitActor->ActorHasTag(FName("Destroyable")))
		{
			UGameplayStatics::SpawnDecalAttached(
				MetalDecalMaterial,
				FVector(10.0f, 10.0f, 10.0f),
				HitComponent,
				FName("None"),
				HitLocation,
				HitRotationFromX,
				EAttachLocation::KeepWorldPosition,
				6.0f
			);
			UGameplayStatics::SpawnEmitterAtLocation(
				this,
				MetalParticleSystem,
				HitLocation,
				HitRotationFromX,
				true
			);
		}
		else
		{
			UGameplayStatics::SpawnDecalAttached(
				ConcreteDecalMaterial,
				FVector(10.0f, 10.0f, 10.0f),
				HitComponent,
				FName("None"),
				HitLocation,
				HitRotationFromX,
				EAttachLocation::KeepWorldPosition,
				6.0f
			);
			UGameplayStatics::SpawnEmitterAtLocation(
				this,
				ConcreteParticleSystem,
				HitLocation,
				HitRotationFromX,
				true
			);
		}
	}
}

FRotator AWeaponBaseActor::GetSpreadAngle(FVector MuzzleDirection, float ConeHalfAngleInDegrees)
{
	float ConeHalfAngleInRadians = FMath::DegreesToRadians(ConeHalfAngleInDegrees);
	FVector Direction = FMath::VRandCone(MuzzleDirection, ConeHalfAngleInRadians);
	return FRotationMatrix::MakeFromX(Direction).Rotator();
}

