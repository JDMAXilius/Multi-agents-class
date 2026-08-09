
#pragma once

#include "CoreMinimal.h"
#include "Data/Enumeration/WeaponEnums.h"
#include "Data/Enumeration/PlayerEnums.h"
#include "WeaponStructs.generated.h"

USTRUCT(BlueprintType)
struct FWeaponData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWeaponName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName PrimaryWeaponSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SecondaryWeaponSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName PrimaryWeaponStoredSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TemporaryWeaponSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName MuzzleSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWeaponType Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAmmoType AmmoType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ELocomotionState PlayerLocomotionType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int AmmoLoaded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int AmmoClipSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FireRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FiringRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxBulletSpreadAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* DryFireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UParticleSystem* MuzzleEffect;
};

USTRUCT(BlueprintType)
struct FPickupData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPickupType Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Amount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float InteractionTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description;
};