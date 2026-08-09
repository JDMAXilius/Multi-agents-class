
#pragma once

#include "CoreMinimal.h"
#include "PlayerEnums.generated.h"

UENUM(BlueprintType)
enum class ELocomotionState : uint8
{	Unarmed    UMETA(DisplayName = "Unarmed"),
	Rifle      UMETA(DisplayName = "Rifle"),
	Pistol     UMETA(DisplayName = "Pistol"),
	Shotgun    UMETA(DisplayName = "Shotgun"),
	Knife      UMETA(DisplayName = "Knife"),
	Bow        UMETA(DisplayName = "Bow"),
	Grenade    UMETA(DisplayName = "Grenade")
};

UENUM(BlueprintType)
enum class EGait : uint8
{
	Jogging     UMETA(DisplayName = "Jogging"),
	Walking     UMETA(DisplayName = "Walking"),
	Crouching   UMETA(DisplayName = "Crouching")
};

UENUM(BlueprintType)
enum class ELoadingNotificationType : uint8
{
	Reloading          UMETA(DisplayName = "Reloading"),
	ApplyBandage       UMETA(DisplayName = "ApplyBandage"),
	UseEnergyDrink     UMETA(DisplayName = "UseEnergyDrink"),
	ThrowGrenade       UMETA(DisplayName = "ThrowGrenade")
};

UENUM(BlueprintType)
enum class EEmptyNotificationType : uint8
{
	None             UMETA(DisplayName = "None"),
	Ammo             UMETA(DisplayName = "Ammo"),
	Bandage          UMETA(DisplayName = "Bandage"),
	EnergyDrink      UMETA(DisplayName = "EnergyDrink"),
	Grenade          UMETA(DisplayName = "Grenade")
};

UENUM(BlueprintType)
enum class EWeaponSocket : uint8
{
	PrimaryWeaponSocket                   UMETA(DisplayName = "PrimaryWeaponSocket"),
	PrimaryWeaponStoredSocket_Rifle       UMETA(DisplayName = "PrimaryWeaponStoredSocket_Rifle"),
	SecondaryWeaponStoredSocket_Rifle     UMETA(DisplayName = "SecondaryWeaponStoredSocket_Rifle"),
	PrimaryWeaponStoredSocket_Pistol      UMETA(DisplayName = "PrimaryWeaponStoredSocket_Pistol"),
	SecondaryWeaponStoredSocket_Pistol    UMETA(DisplayName = "SecondaryWeaponStoredSocket_Pistol")
};

UENUM(BlueprintType)
enum class EPickupInteractNotificationType : uint8
{
	Add          UMETA(DisplayName = "Add"),
	Start        UMETA(DisplayName = "Start"),
	Cancel       UMETA(DisplayName = "Cancel"),
	Remove       UMETA(DisplayName = "Remove")
};

UENUM(BlueprintType)
enum class EOverlayStates : uint8
{
	None           UMETA(DisplayName = "None"),
	Grenade        UMETA(DisplayName = "Grenade"),
	Knife          UMETA(DisplayName = "Knife"),
	Bow            UMETA(DisplayName = "Bow"),
	Sword          UMETA(DisplayName = "Sword")
};