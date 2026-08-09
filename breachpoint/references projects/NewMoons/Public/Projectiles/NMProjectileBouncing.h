// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NMProjectileBase.h"
#include "GameFramework/Actor.h"
#include "NMProjectileBouncing.generated.h"

class UNiagaraSystem;

UCLASS()
class NEWMOONS_API ANMProjectileBouncing : public ANMProjectileBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ANMProjectileBouncing();

protected:
	virtual void BeginPlay() override;

	// Movement variables
	UPROPERTY(Replicated)
	FVector CurrentVelocity;

	UPROPERTY(EditAnywhere, Category = "Grenade | Physics")
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Grenade | Physics")
	float MinBounceSpeed = 150.0f;

	// Segment handling for prediction
	FVector PredictNextLocation(float DeltaTime) const;

	// Explosion properties
	UPROPERTY(EditDefaultsOnly, Category = "Grenade | Explosion")
	float ExplosionRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade | Explosion")
	float ExplosionDamage = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade | Physics")
	float Bounciness = 0.6f;  // Controls the bounce energy retention

	UPROPERTY(EditDefaultsOnly, Category = "Grenade | Physics")
	float Friction = 0.3f;  // Controls the energy lost due to friction

	UPROPERTY(EditAnywhere, Category = "Grenade | Physics")
	float EnergyLossFactor = 0.8f;  // Retain 80% of velocity after each bounce

	UPROPERTY(EditAnywhere, Category = "Grenade | Physics")
	float ExplosionTimer = 3.f;

	bool bFirstBounce = true;
	
	UFUNCTION()
	void Explode();
	
	virtual void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, 
					   UPrimitiveComponent* OtherComp, FVector NormalImpulse, 
					   const FHitResult& Hit) override;

	void CalculateBounce(FVector& Velocity, const FHitResult& Hit);
	
	// Replication function to notify clients about the explosion
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Explode();

	// Handle collision and bouncing
	void HandleCollision(const FHitResult& Hit);

	// Helper functions for trajectory and bounce
	FVector CalculateBouncing(const FVector& Velocity, const FHitResult& Hit) const;

	FVector ApplyGravity(const FVector& Velocity, float DeltaTime) const;

	// Movement update function (replaces Tick)
	void UpdateMovement();

	// Timer for movement updates
	FTimerHandle MovementTimerHandle;

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	// Timer handle for explosion scheduling
	FTimerHandle ExplosionTimerHandle;
};
