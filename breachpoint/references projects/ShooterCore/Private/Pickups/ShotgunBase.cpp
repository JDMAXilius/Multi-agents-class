
#include "Pickups/ShotgunBase.h"
#include "Main/PlayerCharacter.h"
#include "Component/Inventory.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Camera/CameraComponent.h"
#include "Actors/BulletBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/ProjectileMovementComponent.h"

AShotgunBase::AShotgunBase() //: AWeaponBaseActor()
{
	WeaponData.AmmoClipSize = 6;
	WeaponData.AmmoLoaded = 12;
	WeaponData.Damage = 40.0f;
	WeaponData.FireRate = 1.0f;
	WeaponData.FiringRange = 7000.0f;
	WeaponData.MaxBulletSpreadAngle = 6.0f;
	PelletCount = 6;
	MaxBulletSpreadWhenAiming = 1.5f;

	WeaponData.MuzzleSocketName = FName("MuzzleSocket");
	WeaponData.PrimaryWeaponSocketName = FName("PrimaryWeaponSocket_Rifle");
	WeaponData.SecondaryWeaponSocketName = FName("SecondaryWeaponSocket_Rifle");
	WeaponData.PrimaryWeaponStoredSocketName = FName("PrimaryWeaponStoredSocket_Rifle");
	WeaponData.TemporaryWeaponSocketName = FName("TemporaryWeaponSocket_Rifle");
}

void AShotgunBase::Shoot(APlayerCharacter* Player, bool bAdsButtonPressed)
{
	if (WeaponData.AmmoLoaded > 0)
	{
		FVector MuzzleSocketLocation = Mesh->GetSocketLocation(WeaponData.MuzzleSocketName);
		FRotator MuzzleSocketRotation = Mesh->GetSocketRotation(WeaponData.MuzzleSocketName);
		WeaponData.AmmoLoaded = WeaponData.AmmoLoaded - 1;
		UCameraComponent* PlayerCamera = Player->GetPlayerCamera();
		FVector TraceStartLocation = PlayerCamera->GetComponentLocation();
		FVector AimDirection = PlayerCamera->GetForwardVector();

		const float SpreadAngle = bAdsButtonPressed ? MaxBulletSpreadWhenAiming : WeaponData.MaxBulletSpreadAngle; 
		UGameplayStatics::SpawnEmitterAttached(WeaponData.MuzzleEffect, Mesh, WeaponData.MuzzleSocketName, MuzzleSocketLocation, MuzzleSocketRotation, FVector(0.5f, 0.5f, 0.5f), EAttachLocation::KeepWorldPosition);
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData.FireSound, MuzzleSocketLocation);
		Player->PlayCameraShake();

		for (int32 i = 0; i < PelletCount; ++i) 
		{
			FVector PelletDir = GetSpreadAngle(AimDirection, SpreadAngle).Vector(); 
			FVector TraceEndLocation = TraceStartLocation + (PelletDir * WeaponData.FiringRange); 
			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);

			bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStartLocation, TraceEndLocation, ECollisionChannel::ECC_Visibility, QueryParams);

			FTransform SpawnBulletTransform;
			FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(MuzzleSocketLocation, (bHit) ? HitResult.ImpactPoint : HitResult.TraceEnd);
			SpawnBulletTransform.SetLocation(MuzzleSocketLocation);
			SpawnBulletTransform.SetRotation(LookAtRotation.Quaternion());
			SpawnBulletTransform.SetScale3D(FVector(1.0f, 1.0f, 1.0f));

			ABulletBase* BulletActor = GetWorld()->SpawnActor<ABulletBase>(BulletClass, SpawnBulletTransform);
			BulletActor->AmmoType = WeaponData.AmmoType;
			BulletActor->SetOwner(this);

			HandleImpact(HitResult, WeaponData.Damage / PelletCount); 
		}

		SpawnBulletShell(Player);
	}
	else
	{
		Player->PushEmptyNotification(EEmptyNotificationType::Ammo);
		UGameplayStatics::PlaySound2D(this, WeaponData.DryFireSound);
	}
}

