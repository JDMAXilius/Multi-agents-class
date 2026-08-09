
#pragma once

#include "CoreMinimal.h"
#include "WeaponEnums.generated.h"


UENUM(BlueprintType)
enum class EAmmoType : uint8
{
	Pistol         UMETA(DisplayName = "Pistol"),
	Rifle          UMETA(DisplayName = "Rifle"),
	Shotgun        UMETA(DisplayName = "Shotgun")
};

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Pistol     UMETA(DisplayName = "Pistol"),
	Rifle      UMETA(DisplayName = "Rifle"),
	Shotgun    UMETA(DisplayName = "Shotgun"),
	Knife      UMETA(DisplayName = "Knife"),
	Bow        UMETA(DisplayName = "Bow")
};

UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	Primary       UMETA(DisplayName = "Primary"),
	Secondary     UMETA(DisplayName = "Secondary")
};

UENUM(BlueprintType)
enum class EWeaponState :uint8
{
	Dropped       UMETA(DisplayName = "Dropped"),
	Equipped      UMETA(DisplayName = "Equipped")
};

UENUM(BlueprintType)
enum class EWeaponName : uint8
{
	Pistol01                UMETA(DisplayName = "Pistol01"),
	Pistol02                UMETA(DisplayName = "Pistol02"),
	Pistol03                UMETA(DisplayName = "Pistol03"),
	Rifle01                 UMETA(DisplayName = "Rifle01"),
	Rifle02                 UMETA(DisplayName = "Rifle02"),
	Rifle03                 UMETA(DisplayName = "Rifle03"),
	Rifle04                 UMETA(DisplayName = "Rifle04"),
	Rifle05                 UMETA(DisplayName = "Rifle05"),
	Rifle06                 UMETA(DisplayName = "Rifle06"),
	Rifle07                 UMETA(DisplayName = "Rifle07"),
	Rifle08                 UMETA(DisplayName = "Rifle08"),
	Shotgun01               UMETA(DisplayName = "Shotgun01"),
	Shotgun02               UMETA(DisplayName = "Shotgun02")
};

UENUM(BlueprintType)
enum class EPickupType : uint8
{
	Ammo          UMETA(DisplayName = "Ammo"),
	Bagpack       UMETA(DisplayName = "Bagpack"),
	Grenade       UMETA(DisplayName = "Grenade"),
	HealthKit     UMETA(DisplayName = "HealthKit"),
	Weapon        UMETA(DisplayName = "Weapon")
};

UENUM(BlueprintType)
enum class EHealthKitType : uint8
{
	Bandage          UMETA(DisplayName = "Bandage"),
	EnergyDrink       UMETA(DisplayName = "EnergyDrink")
};