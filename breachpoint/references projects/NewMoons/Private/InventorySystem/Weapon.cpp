// Fill out your copyright notice in the Description page of Project Settings.


#include "InventorySystem/Weapon.h"
#include "Projectiles/Bullet.h"
#include "Sound/SoundCue.h"
#include "Characters/NMCharacterBase.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

// Sets default values
AWeapon::AWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Ammo(20)
	, MagazineCapacity(2)
	, BarrelSocketName(FName{ "BarrelSocket" })
	, AutomaticFireRate(0.1f)
	, bShouldFire(true)
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;


	SkeletalMeshComponent->SetEnableGravity(false);
}

// Called when the game starts or when spawned
void AWeapon::BeginPlay()
{
	Super::BeginPlay();
	
}

void AWeapon::AutoFireReset()
{
	bShouldFire = true;
}

void AWeapon::Fire()
{
	if (!bShouldFire)
	{
		return;
	}

	const USkeletalMeshSocket* BarrelSocket = SkeletalMeshComponent->GetSocketByName(BarrelSocketName);

	if (!BarrelSocket)
	{
		return;
	}

	const FVector BarrelSocketLocation = BarrelSocket->GetSocketLocation(SkeletalMeshComponent);

	// Trace from the camera location to the middle of the screen
	FHitResult OutHitResult;
	ANMCharacterBase* Character = Cast<ANMCharacterBase>(GetOwner());
	const FVector Start{ Character->GetCamera()->GetComponentLocation()};
	FVector End{ Start + Character->GetCamera()->GetForwardVector() * 50'000.f };
	GetWorld()->LineTraceSingleByChannel(OutHitResult, Start, End, ECollisionChannel::ECC_Visibility);

	FTransform SpawnTransform;
	SpawnTransform.SetLocation(BarrelSocketLocation);

	// Account as well for the position of the Muzzle
	if (OutHitResult.bBlockingHit)
	{
		SpawnTransform.SetRotation((OutHitResult.Location - BarrelSocketLocation).ToOrientationQuat());
		End = OutHitResult.Location;

		// Spawn the impact vfx if we hit anything
		if (ImpactVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactVFX, OutHitResult.Location, OutHitResult.ImpactNormal.ToOrientationRotator());
		}
	}
	else
	{
		SpawnTransform.SetRotation(Character->GetCamera()->GetForwardVector().ToOrientationQuat());
	}

	// Spawn the bullet
	if (IsValid(DefaultBulletClass))
	{
		const FActorSpawnParameters SpawnParams;
		GetWorld()->SpawnActor<ABullet>(DefaultBulletClass, SpawnTransform, SpawnParams);
	}

	// Play fire sound
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), FireSound, BarrelSocketLocation);
	}

	// Play fire montage
	//if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance(); 
	//	AnimInstance && Character->GetFireMontage())
	//{
	//	AnimInstance->Montage_Play(Character->GetFireMontage());
	//}

	// Spawn the muzzle flash
	if (MuzzleFlashVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(MuzzleFlashVFX, SkeletalMeshComponent, BarrelSocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}

	// Spawn trail until target location
	if (BeamVFX)
	{
		UNiagaraComponent* Beam = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BeamVFX, BarrelSocketLocation, SpawnTransform.GetRotation().Rotator());
		Beam->SetVectorParameter(FName("EndLocation"), End);
		Beam->SetFloatParameter(FName("Length"), (End - BarrelSocketLocation).Length());
	}

	bShouldFire = false;
	GetWorldTimerManager().SetTimer(AutoFireTimer, this, &ThisClass::AutoFireReset, AutomaticFireRate);
}

// Called every frame
void AWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}