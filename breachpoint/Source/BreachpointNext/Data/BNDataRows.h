#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BNDataRows.generated.h"

class UAnimInstance;
class UAnimMontage;
class UBNAbilitySet;
class USkeletalMesh;
class USoundBase;
class UTexture2D;
class ABNWeapon;

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

	// ---- positional damage. Zorans' bone→section shape (ZoransCharacterBase.cpp:157), reduced to
	// the four sections a Manny actually has. HeadshotMultiplier keeps its name and its meaning:
	// it IS the head section's multiplier, so no row that already sets it changes behaviour.
	// The other three default to 1.0 — landing this changes NOTHING until a row is tuned, which
	// is the point: the capability is data, the decision is yours.
	UPROPERTY(EditAnywhere, Category = "Combat")
	float HeadshotMultiplier = 2.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float TorsoMultiplier = 1.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ArmMultiplier = 1.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float LegMultiplier = 1.f;

	// ---- distance falloff. Lyra evaluates a rich curve per weapon
	// (LyraRangedWeaponInstance.cpp:125); three numbers express the same shape without a curve
	// asset per weapon, and a DataTable can hold them. DISABLED by default (End = 0), so a row
	// that says nothing keeps doing full damage at every range, exactly as today.
	//
	//   distance <= Start        -> full damage
	//   Start .. End             -> linear down to MinMultiplier
	//   distance >= End          -> MinMultiplier
	UPROPERTY(EditAnywhere, Category = "Combat")
	float FalloffStartDistance = 0.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float FalloffEndDistance = 0.f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FalloffMinMultiplier = 1.f;

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

	/** A knife does not aim. UBNGA_ADS refuses activation when the current row says no. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bCanADS = true;

	/** Melee is its own damage number, not the shot's: a rifle butt has no headshot rule and no
	 *  falloff, so it goes through BNDamage's flat door rather than the weapon-shaped one. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float MeleeDamage = 40.f;

	/** Reach. `MyCharacter::MeleeTraceDistance` measured 120 (.h:359); the trace is data here for
	 *  the same reason Range is — a literal in the ability would need a rebuild to tune. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float MeleeRange = 120.f;

	UPROPERTY(EditAnywhere, Category = "Ammo")
	int32 MagazineSize = 30;

	/** The pool behind the magazine. Reload TRANSFERS from here (min(MagSize - InMag, Reserve) —
	 *  UBRWeaponInstance's proven math); before this field existed, Reload() refilled the magazine
	 *  from nothing and ammo was silently infinite. Knife: 0 and 0. */
	UPROPERTY(EditAnywhere, Category = "Ammo")
	int32 ReserveAmmo = 90;

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

	/** What ABNWeapon a subclass-through-data spawn produces. None = plain ABNWeapon, which every
	 *  current weapon uses — the hierarchy exists the moment a weapon's BEHAVIOR (not values)
	 *  differs, and not one day before (RESEARCH-WEAPON-BASE). */
	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftClassPtr<ABNWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftObjectPtr<USkeletalMesh> WeaponMesh;

	/** The HUD's weapon silhouette (R7.1). Soft, and read ONLY by the UI — the design's tray puts
	 *  an 88×32 line-art gun beside the ammo, and without this the slot has nothing to draw. A row
	 *  that leaves it unset renders no silhouette and nothing else changes: the ammo, the name and
	 *  the reserve are their own fields. */
	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UTexture2D> Icon;

	/** The centre reticle for THIS weapon. Soft, UI-only, and separate from `Icon` on purpose:
	 *  the silhouette is a side-on line-art gun and the reticle is a sight, so one column cannot
	 *  serve both. A row that leaves it unset falls back to the HUD's default reticle rather
	 *  than drawing nothing — an FPS with no aiming mark is a bug, never a designed miss. */
	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UTexture2D> Reticle;

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

	/** The gunshot. Per weapon, so it belongs on the row rather than on the cue: the rifle and the
	 *  pistol are two sounds and one cue class serves both. The muzzle cue reads it off the firing
	 *  weapon and falls back to its own Config sound when a row leaves it empty. Reload needs no
	 *  twin — that audio rides the reload montage's own notifies, which is why reload was already
	 *  audible while the shot was silent. */
	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftObjectPtr<USoundBase> FireSound;

	/** The swing. Its connect window is the montage's own `AN_FPST_Melee` notify — the record
	 *  notes that is the one melee notify the template ships, so it is reused rather than
	 *  re-authored. With no montage the ability swings immediately and still damages. */
	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UAnimMontage> MeleeMontage;

	// The weapon's own verbs, granted on equip and revoked on swap, authority only. Precedent:
	// the old module's Config/DefaultGame.ini — "Fire, Reload and Swap live in the WEAPON's
	// ability set (DT_Weapons column AbilitySet), not in StartupAbilitySet".
	UPROPERTY(EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UBNAbilitySet> AbilitySet;
};

