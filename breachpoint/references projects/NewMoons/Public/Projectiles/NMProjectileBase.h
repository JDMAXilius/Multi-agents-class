// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "NMProjectileBase.generated.h"

class UProjectileMovementComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;

UCLASS()
class NEWMOONS_API ANMProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ANMProjectileBase();

	// Sphere collision component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Porperties | Components")
	USphereComponent* CollisionComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Porperties | Components")
	UStaticMeshComponent* ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Porperties | Components")
	UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Porperties | Projectile Attributes")
	UNiagaraSystem* TrailEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Porperties | Projectile Attributes")
	UNiagaraSystem* ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Porperties | Projectile Attributes")
	USoundBase* ExplosionSound;
	
	// If we spawn from a weapon, this would typically be "Muzzle". If not spawned from a weapon, it will assume it is from the character mesh and find that socket/bone location.
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Porperties | Projectile Attributes")
	FName SpawnSocketName = FName("MuzzleSocket");

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Porperties | Projectile Attributes")
	bool IsDebugsActive = false;
	
	// Initialize projectile with speed, damage, and owner’s controller
	void InitializeProjectile(float Speed, AActor* InOwnerActor, AActor* InWeaponActor);
	void FireInDirection(FVector InLaunchDirection);
	void PredictProjectilePath(int32 MaxSimulatedBounces, float SimulationTimeStep, float TotalSimulationTime);
	void SetInitialVelocity(const FVector& Velocity);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Porperties | Projectile")
	float BaseDamage = 20.0f;

	UPROPERTY()
	AController* InstigatorController;

	// Owner and weapon to ignore during collision
	AActor* OwnerActor = nullptr;
	AActor* WeaponActor = nullptr;
	
	// Handles impact on collision
	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, 
					   FVector NormalImpulse, const FHitResult& Hit);
private:
	
	void HandleImpact(AActor* HitActor);
};
