#pragma once

#include "CoreMinimal.h"
#include "PlayerStructs.generated.h"

USTRUCT(BlueprintType)
struct FGaitSetting
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxWalkSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxAcceleration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BrakingDeceleration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BrakingFrictionFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BrakingFriction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseSeparateBrakingFriction;

};

USTRUCT(BlueprintType)
struct FWeaponSocketController
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPrimaryWeaponSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPrimaryWeaponStoredSocket_Rifle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSecondaryWeaponStoredSocket_Rifle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPrimaryWeaponStoredSocket_Pistol;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSecondaryWeaponStoredSocket_Pistol;

};