
//Base bullet actor class responsible for bullet visuals and movement

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/Enumeration/WeaponEnums.h"
#include "BulletBase.generated.h"

// Forward declarations
class UParticleSystemComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UParticleSystem;

UCLASS()
class SHOOTERCORE_API ABulletBase : public AActor
{
	GENERATED_BODY()
	
public:	
	ABulletBase();
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data")
	EAmmoType AmmoType;

protected:
	virtual void BeginPlay() override;

private:
	// Mesh representing the bullet shape
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Scene", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BulletMesh;

	// Particle trail component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|ParticleSystem", meta = (AllowPrivateAccess = "true"))
	UParticleSystemComponent* ParticleSystem;

	// Handles bullet movement and velocity
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|ProjectileMovement", meta = (AllowPrivateAccess = "true"))
	UProjectileMovementComponent* ProjectileMovement;

	// Trail effect for rifle bullets
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|ParticleSystem", meta = (AllowPrivateAccess = "true"))
	UParticleSystem* RifleAmmo;

	// Trail effect for pistol bullets
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|ParticleSystem", meta = (AllowPrivateAccess = "true"))
	UParticleSystem* PistolAmmo;

	// Trail effect for shotgun bullets
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|ParticleSystem", meta = (AllowPrivateAccess = "true"))
	UParticleSystem* ShotgunAmmo;
};
