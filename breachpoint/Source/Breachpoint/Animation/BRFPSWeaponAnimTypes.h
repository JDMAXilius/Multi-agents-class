#pragma once

#include "CoreMinimal.h"

#include "BRFPSWeaponAnimTypes.generated.h"

/**
 * The weapon vocabulary the ANIMATION side needs — and deliberately nothing more.
 *
 * SCOPE, STATED FIRST, BECAUSE THIS FILE IS EASY TO GROW WRONG. `BP_FPST_BaseWeapon` carried 124
 * properties and its four subclasses added **zero** members between them, differing only in 6–20
 * defaults (`docs/ANIM-PORT-LEDGER.md`). That is a data table wearing four class hats, and the
 * weapon system it implies — slots, ammo, pickups, equip — is **`BP97`'s `BREquipmentComponent`
 * and `BRWeaponInstance`**, not this ticket's and not this folder's.
 *
 * What lives here is only the part the SPINE has to know to pose a weapon:
 *   · how the weapon can fire, because it changes the pose, not because it changes the damage
 *   · where the weapon attaches and where its muzzle is, because those are skeleton facts
 *   · which crosshair it wants, which is UI's, carried here only because the inventory had it
 *     on the weapon and dropping it silently would lose the evidence
 *
 * WHAT IS NOT HERE, and each has a home: `fireDelay` 0.1, `reloadBoltDelay` 0.4, `spreadAngle`
 * 0.1, `shotCount` 1 are **tuning numbers** and law 3 puts them in `Content/Data` CSVs with a row
 * struct in `Data/BRDataRows.h` — a folder this ticket does not own. They are recorded in the
 * ledger so BP97 inherits the measurements rather than re-deriving them.
 */

/**
 * How the trigger behaves. From `availableFireModes` (`["Single"]` on the base) and
 * `curFireModeIndex`.
 *
 * An enum, not the template's string array: a fire mode is a closed set, and a string comparison
 * to decide a pose is a lookup that can fail at runtime instead of at compile time.
 */
UENUM(BlueprintType)
enum class EBRFPSFireMode : uint8
{
	/** One shot per pull. */
	Single,

	/** Held trigger, cadence from the weapon's row. */
	Automatic,

	/** Fixed count per pull. */
	Burst
};

/** From `crosshairType` ("Rifle" on the base). UI's concern; kept so the evidence is not lost. */
UENUM(BlueprintType)
enum class EBRFPSCrosshairType : uint8
{
	None,
	Rifle,
	Pistol,
	Shotgun,
	Melee
};

/**
 * Skeleton attachment facts for one weapon.
 *
 * These are the two names in the inventory that are genuinely STRUCTURAL rather than tuning:
 * `attachSocketName` ("hand_r") and `muzzleSocketName` ("Muzzle"). They are not numbers, so law 3
 * does not send them to a CSV; they are the contract between a weapon mesh and a skeleton, and
 * getting one wrong attaches the gun to the wrong bone — a failure that is obvious on sight,
 * which is exactly why it should be declared once instead of typed at each attach site.
 */
USTRUCT(BlueprintType)
struct FBRFPSWeaponSockets
{
	GENERATED_BODY()

	/** Where the weapon attaches on the character skeleton. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Weapon")
	FName AttachSocket = TEXT("hand_r");

	/** Where shots, muzzle flash and tracers originate, on the WEAPON's skeleton. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Weapon")
	FName MuzzleSocket = TEXT("Muzzle");

	/** Where the off hand is placed when the layer does not pose it. Empty means "the layer does". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Weapon")
	FName LeftHandSocket = NAME_None;
};
