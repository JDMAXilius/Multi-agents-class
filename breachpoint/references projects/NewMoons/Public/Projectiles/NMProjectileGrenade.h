// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NMProjectileBase.h"
#include "GameFramework/Actor.h"
#include "NMProjectileGrenade.generated.h"

class UNiagaraSystem;
class ANMCharacterBase;

UCLASS()
class NEWMOONS_API ANMProjectileGrenade : public ANMProjectileBase
{
	GENERATED_BODY()

	ANMProjectileGrenade();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Porperties | Grenade | Explosion")
	float ExplosionDamage = 50.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Porperties | Grenade | Physics")
	float Bounciness = 0.6f;  // Controls the bounce energy retention
	UPROPERTY(EditDefaultsOnly, Category = "Porperties | Grenade | Physics")
	float Friction = 0.2f;  // Controls the energy lost due to friction
	UPROPERTY(EditAnywhere, Category = "Porperties | Grenade | Physics")
	float EnergyLossFactor = 0.8f;  // Retain 80% of velocity after each bounce
	UPROPERTY(EditAnywhere, Category = "Porperties | Grenade | Physics")
	float ExplosionTimer = 3.f;

	bool bFirstBounce = true;
	
private:

	// Explosion properties
	UPROPERTY(EditAnywhere, Category = "Porperties | Grenade | Explosion")
	float ExplosionRadius = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Porperties | Grenade | Explosion")
	float MaxDamage = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Porperties | Grenade | Explosion")
	float MinDamage = 10.0f;

	// Bounce properties
	UPROPERTY(EditAnywhere, Category = "Porperties | Grenade | Bounces")
	int32 MaxBounces = 3;

	int32 CurrentBounces;

	// Timer for explosion
	FTimerHandle ExplosionTimerHandle;

	// Utility functions
	void Explode();
	
	float CalculateDamage(const FVector& ExplosionOrigin, const FVector& TargetLocation) const;

	virtual void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, 
						   UPrimitiveComponent* OtherComp, FVector NormalImpulse, 
						   const FHitResult& Hit) override;
	void CalculateBounceForPMC(FVector& Velocity, const FHitResult& Hit);
	
	// Replication
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Explode();
};