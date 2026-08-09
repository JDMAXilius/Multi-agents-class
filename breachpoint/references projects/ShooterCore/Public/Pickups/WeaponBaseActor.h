
#pragma once

#include "CoreMinimal.h"
#include "Pickups/PickupBase.h"
#include "Data/Enumeration/WeaponEnums.h"
#include "Data/Structure/WeaponStructs.h"
#include "WeaponBaseActor.generated.h"


class APlayerCharacter;
class ABulletBase;
class ABulletShellBase;
class UMaterialInterface;
class UParticleSystem;

UCLASS()
class SHOOTERCORE_API AWeaponBaseActor : public APickupBase
{
	GENERATED_BODY()

public:
	AWeaponBaseActor();
	void Drop();
	virtual void Shoot(APlayerCharacter* Player, bool bAdsButtonPressed);
	bool CheckAmmo();
	void ReloadingWeapon();
	void SpawnBulletShell(AActor* Actor);
	void ApplyForwardImpulse(FVector ImpulseDirection);

protected:
	virtual void BeginPlay() override;
	virtual void InteractedToPickup_Implementation(APlayerCharacter* Player) override;
	void SetWeaponState(EWeaponState State);
	void Reload(int& Ammo);
	void HandleImpact(FHitResult& Hit, float Damage);
	FRotator GetSpreadAngle(FVector MuzzleDirection, float ConeHalfAngleInDegrees);

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data|Weapon")
	FWeaponData WeaponData;

protected:
	UPROPERTY(BlueprintReadOnly)
	EWeaponState CurrentWeaponState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Weapon|Bullet", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ABulletBase> BulletClass;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Shell", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ABulletShellBase> BulletShellClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Shell", meta = (AllowPrivateAccess = "true"))
	FName ShellSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Weapon|DecalMaterial", meta = (AllowPrivateAccess = "true"))
	UMaterialInterface* ConcreteDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Weapon|DecalMaterial", meta = (AllowPrivateAccess = "true"))
	UMaterialInterface* BloodDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Weapon|DecalMaterial", meta = (AllowPrivateAccess = "true"))
	UMaterialInterface* MetalDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Weapon|ParticleSystem", meta = (AllowPrivateAccess = "true"))
	UParticleSystem* ConcreteParticleSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Weapon|ParticleSystem", meta = (AllowPrivateAccess = "true"))
	UParticleSystem* BloodParticleSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Weapon|ParticleSystem", meta = (AllowPrivateAccess = "true"))
	UParticleSystem* MetalParticleSystem;
	
};
