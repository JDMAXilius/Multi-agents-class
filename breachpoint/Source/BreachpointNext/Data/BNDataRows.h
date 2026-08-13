#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BNDataRows.generated.h"

class UAnimInstance;
class UAnimMontage;
class UBNAbilitySet;
class USkeletalMesh;

// Our own enum, not the template's E_FPST_FireMode asset enum. Same three modes in the same
// order (MyCharacter.cpp:74-77), so an imported number still means what it meant.
UENUM(BlueprintType)
enum class EBNFireMode : uint8
{
	Single,
	Auto,
	Burst
};

// EditAnywhere, not EditDefaultsOnly: a DataTable row on disk IS an instance, and
// EditDefaultsOnly greys the field out in the table editor and blocks scripted writes.
USTRUCT(BlueprintType)
struct FBNWeaponRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Damage = 20.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float HeadshotMultiplier = 2.f;

	// Seconds between shots, not rounds-per-minute: the template's `FireDelay` variable is
	// already a period and feeds the fire timer directly (MyCharacter.h:143-149, .cpp:1685).
	UPROPERTY(EditAnywhere, Category = "Combat")
	float FireDelay = 0.1f;

	// The template's `SpreadAngle` — the half-angle of the shot cone (MyCharacter.cpp:1797-1812).
	UPROPERTY(EditAnywhere, Category = "Combat")
	float SpreadAngle = 0.f;

	// Pellets per trigger pull. The template's `ShotCount`: 1 on every weapon except the
	// shotgun, which is 6 (MyCharacter.h:151-155, .cpp:1777-1795).
	UPROPERTY(EditAnywhere, Category = "Combat")
	int32 ShotCount = 1;

	// The template's `GetCurrentFireMode` byte, retyped (MyCharacter.h:157-161, .cpp:1814-1826).
	UPROPERTY(EditAnywhere, Category = "Combat")
	EBNFireMode FireMode = EBNFireMode::Single;

	// Read only when FireMode is Burst; the burst arm counts to it (MyCharacter.cpp:1701-1709).
	UPROPERTY(EditAnywhere, Category = "Combat")
	int32 BurstShotCount = 3;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Range = 10000.f;

	UPROPERTY(EditAnywhere, Category = "Ammo")
	int32 MagazineSize = 30;

	UPROPERTY(EditAnywhere, Category = "Ammo")
	float ReloadTime = 2.f;

	// The template's `AttachSocketName` — a socket or bone on the CHARACTER mesh
	// (MyCharacter.cpp:50, 632, 657-683).
	UPROPERTY(EditAnywhere, Category = "Attachment")
	FName AttachSocketName;

	// The template's `MuzzleSocketName` — a socket on the WEAPON's own mesh, not the
	// character's (MyCharacter.cpp:52, 1557-1580).
	UPROPERTY(EditAnywhere, Category = "Attachment")
	FName MuzzleSocketName;

	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftObjectPtr<USkeletalMesh> WeaponMesh;

	// The template's `LinkAnimLayerClass`, read off the weapon by reflection
	// (MyCharacter.cpp:51, 2028-2031). G2.4 resolves it into the linked layer.
	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftClassPtr<UAnimInstance> AnimLayerClass;

	// The template's `FireAnimMontage` / `ReloadAnimMontage` weapon variables
	// (MyCharacter.cpp:57-61, played through GetWeaponMontage at :1228-1265).
	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UAnimMontage> ReloadMontage;

	// The weapon's own verbs, granted on equip and revoked on swap, authority only. Precedent:
	// the old module's Config/DefaultGame.ini — "Fire, Reload and Swap live in the WEAPON's
	// ability set (DT_Weapons column AbilitySet), not in StartupAbilitySet".
	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UBNAbilitySet> AbilitySet;
};
