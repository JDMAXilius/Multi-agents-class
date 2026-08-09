// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Bullet.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class NEWMOONS_API ABullet : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABullet();

	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:
	/** Mesh for the bullet */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullet Properties", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;
	/** Collision component for physics and collision detection */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullet Properties", meta = (AllowPrivateAccess = "true"))
	USphereComponent* CollisionSphere;
	/** Component that makes the bullet move */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullet Properties", meta = (AllowPrivateAccess = "true"))
	UProjectileMovementComponent* ProjectileMovement;

	UFUNCTION()
	void OnBulletHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
