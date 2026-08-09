// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractableSkeletalActor.h"
#include "Weapon.generated.h"

class ABullet;
class USoundCue;
class UNiagaraSystem;

UCLASS()
class NEWMOONS_API AWeapon : public AInteractableSkeletalActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWeapon(const FObjectInitializer& ObjectInitializer);

	/** Get Amount of Ammo Left */
	FORCEINLINE int32 GetAmmo() const { return Ammo; }
	/** Get how many ammo the mag can hold */
	FORCEINLINE int32 GetMagazineCapacity() const { return MagazineCapacity; }

	/** Called to fire the weapon*/
	virtual void Fire();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called to reset the weapon fire
	UFUNCTION()
	void AutoFireReset();

private:
	/** Ammo count for this Weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	int32 Ammo;
	/** Maximum ammo that our weapon can hold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	int32 MagazineCapacity;
	/** Socket name to spawn the bullet and cosmetics on the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	FName BarrelSocketName;
	/** Set this in Blueprints for the default Bullet class */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ABullet> DefaultBulletClass;
	/** gunshot sound cue */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	USoundCue* FireSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	UNiagaraSystem* MuzzleFlashVFX;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	UNiagaraSystem* BeamVFX;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	UNiagaraSystem* ImpactVFX;
	/** True when we can fire. False when waiting for the timer */
	bool bShouldFire;
	/** Rate of automatic gun fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0", ForceUnits = "Seconds"))
	float AutomaticFireRate;
	/** Sets a timer between gunshots */
	FTimerHandle AutoFireTimer;
};
