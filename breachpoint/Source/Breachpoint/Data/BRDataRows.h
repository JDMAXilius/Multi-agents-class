// Breachpoint. DataTable row structs — the ONE header for every DT_ row type.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRDataRows.generated.h"

class UStaticMesh;

/**
 * BRDataRows — every `DT_` DataTable row struct in the project lives in this one header
 * (`data-and-assets.md`: "ALL of them live in the one header Source/Breachpoint/Data/BRDataRows.h").
 *
 * APPEND-ONLY, MANY WRITERS. Several packets add a struct here. Add yours at the bottom
 * with its own banner; never reorder, rewrite, or "tidy" a struct you do not own.
 *
 * TWO KINDS OF TABLE, and only one of them belongs here:
 *   - `DT_` DataTables  -> need a USTRUCT row type -> it goes here.
 *   - `CT_` CurveTables (`CT_Combat`) -> named curves, NO row struct -> never here.
 *
 * Law 3: the CSV in `Content/Data/` is the SOURCE OF TRUTH for the schema. A struct member
 * exists because a CSV column exists, spelled identically. Adding a member without adding
 * the column produces a silently-zero field at import; the reverse produces a warning nobody
 * reads. Both are findings.
 */

// ===========================================================================================
// FBRWeaponRow  ->  Content/Data/DT_Weapons.csv          (owner: sim-builder, ticket BP03)
// ===========================================================================================

/**
 * How the trigger behaves — cadence only.
 *
 * Deliberately NOT the same axis as EBRDamageDelivery: the schema split was forced by the
 * verifier in the 29 Jul 2026 data-crew run (`data-and-assets.md`). Conflating them again
 * is a finding.
 *
 * The enumerator names ARE the CSV's accepted spellings (the DataTable importer matches the
 * cell text against the enumerator name), so renaming one breaks the table, not the build.
 */
UENUM(BlueprintType)
enum class EBRWeaponFireMode : uint8
{
	/** Holding the trigger keeps firing at RPM. */
	Automatic,
	/** One shot per press; RPM is the ceiling, not the cadence. */
	SemiAuto
};

/**
 * How the shot reaches the target — delivery only.
 *
 * INVARIANT (asserted at import and in `Breachpoint.Sim.*`, per `data-and-assets.md`):
 *   ProjectileSpeed == 0  IFF  DamageDelivery == Hitscan.
 * See FBRWeaponRow::ValidateSchema().
 */
UENUM(BlueprintType)
enum class EBRDamageDelivery : uint8
{
	/** Instant trace along the muzzle ray. ProjectileSpeed must be 0. */
	Hitscan,
	/** A spawned projectile actor travels at ProjectileSpeed. Must be > 0. */
	Projectile
};

/**
 * FBRWeaponRow — one row of `Content/Data/DT_Weapons.csv` (AR, Magnum, Rocket).
 *
 * Every gameplay number the fire path uses comes from here; `BRGA_WeaponFire` /
 * `BRGA_WeaponUtility` contain ZERO weapon literals (BP03 done-when box 1 is grep-audited).
 *
 * COLUMN MAP — CSV header -> member. The CSV is the source of truth; this list is the proof:
 *
 *   Name             -> (row key)  the DataTable importer consumes column 1 as the row NAME
 *                                  (FName), so it is deliberately NOT a member. `AR`,
 *                                  `Magnum`, `Rocket` are the row handles the rest of the
 *                                  game addresses weapons by.
 *   DisplayName      -> DisplayName
 *   FireMode         -> FireMode
 *   DamageDelivery   -> DamageDelivery
 *   DamagePerShot    -> DamagePerShot
 *   RPM              -> RPM
 *   MagSize          -> MagSize
 *   ReserveMags      -> ReserveMags
 *   ReloadTime_s     -> ReloadTime_s
 *   HeadshotMult     -> HeadshotMult
 *   ProjectileSpeed  -> ProjectileSpeed
 *   SplashRadius_m   -> SplashRadius_m
 *   SplashDamage     -> SplashDamage
 *   EquipTime_s      -> EquipTime_s
 *   MeshSoftPath     -> MeshSoftPath
 *   FireCueTag       -> FireCueTag
 *
 * Member names keep the CSV's `_s` / `_m` unit suffixes on purpose: the suffix IS the unit
 * contract, and dropping it in C++ is how a seconds value ends up multiplied by 1000.
 *
 * UNITS: the table speaks DESIGNER units (metres, seconds); Unreal speaks centimetres.
 * Conversion happens at the use site through the accessors below, never by a bare *100 in
 * ability code.
 */
USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRWeaponRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Player-facing name (HUD, killfeed, pickup prompt). CSV: DisplayName. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	/** Trigger cadence. CSV: FireMode (Automatic | SemiAuto). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EBRWeaponFireMode FireMode = EBRWeaponFireMode::SemiAuto;

	/** How the shot reaches the target. CSV: DamageDelivery (Hitscan | Projectile). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EBRDamageDelivery DamageDelivery = EBRDamageDelivery::Hitscan;

	/**
	 * Base damage of one shot, BEFORE BRDamageExecCalc applies its Damage.* multipliers.
	 * Fed to GE_Damage as SetByCaller.BaseDamage — it is never applied directly. CSV: DamagePerShot.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float DamagePerShot = 0.f;

	/**
	 * Rounds per minute. The server's rate gate is `RPM + tolerance` (BP03 step 2); the
	 * minimum legal interval between two shots is GetMinShotIntervalSeconds(). CSV: RPM.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float RPM = 0.f;

	/** Rounds in a full magazine. CSV: MagSize. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 MagSize = 0;

	/**
	 * Spare magazines carried on pickup; starting reserve = MagSize * ReserveMags.
	 * The Rocket's 0 is load-bearing (ruling R4: reload is intentionally unreachable).
	 * CSV: ReserveMags.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 ReserveMags = 0;

	/**
	 * Seconds from reload start to the Event.Weapon.ReloadCommit notify (ruling R17).
	 * A cancel before that event costs and refunds nothing. CSV: ReloadTime_s.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float ReloadTime_s = 0.f;

	/**
	 * Headshot damage multiplier. 1.0 means "no headshot bonus" and is a DESIGN POSITION for
	 * the AR (R2) and the Rocket (R4), not a missing number. CSV: HeadshotMult.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float HeadshotMult = 1.f;

	/**
	 * Projectile travel speed in METRES per second; 0 for hitscan. See the enum invariant.
	 * CSV: ProjectileSpeed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float ProjectileSpeed = 0.f;

	/** Explosion radius in METRES; 0 = no splash. CSV: SplashRadius_m. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float SplashRadius_m = 0.f;

	/** Damage at the centre of the splash, before falloff (CT_Combat owns the falloff curve). CSV: SplashDamage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float SplashDamage = 0.f;

	/** Seconds to bring the weapon up on a swap — R3's "0.4 s swap" lives here. CSV: EquipTime_s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float EquipTime_s = 0.f;

	/**
	 * SOFT reference to the weapon's world/first-person mesh, resolved by
	 * BREquipmentComponent through the streamable manager at equip time.
	 * Soft is law (`data-and-assets.md`): a hard UPROPERTY asset ref or ConstructorHelpers
	 * here is a finding, and the pre-tool hook blocks the latter outright. CSV: MeshSoftPath.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<UStaticMesh> MeshSoftPath;

	/**
	 * GameplayCue tag played on fire (muzzle + tracer), predicted OnActive by BRGA_WeaponFire.
	 * CSV: FireCueTag.
	 *
	 * KNOWN GAP (BP03 step 1, reported): the three cue tags this column names
	 * (GameplayCue.Weapon.{AR,Magnum,Rocket}.Fire) are NOT declared anywhere yet —
	 * BRGameplayTags.h says outright that §3.1 enumerates no GameplayCue leaves and refuses
	 * to invent them. Until they are registered, this column imports EMPTY with a warning.
	 * FGameplayTag is still the correct type: it makes the omission loud at import instead of
	 * carrying an unvalidated string all the way to a cue that silently never plays.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FGameplayTag FireCueTag;

	// -- Derived accessors -------------------------------------------------------------
	// Pure, deterministic, no world dependency: safe to call from a headless spec.
	// These are UNIT conversions and algebra, not tuning — the numbers all came from the row.

	/** Centimetres per metre. A unit constant, not a gameplay number: it is never tuned. */
	static constexpr float CmPerMetre = 100.f;

	/** Seconds per minute. Same: a unit constant. */
	static constexpr float SecondsPerMinute = 60.f;

	/** Splash radius in Unreal units (cm). */
	float GetSplashRadiusCm() const { return SplashRadius_m * CmPerMetre; }

	/** Projectile speed in Unreal units per second (cm/s). */
	float GetProjectileSpeedCmPerSecond() const { return ProjectileSpeed * CmPerMetre; }

	/**
	 * Shortest legal interval between two shots, in seconds, from RPM alone.
	 * Returns 0 for a row with RPM <= 0 — meaning "no rate gate expressible", which the
	 * server's rate check must treat as a REFUSAL to fire, never as "unlimited".
	 */
	float GetMinShotIntervalSeconds() const
	{
		return (RPM > 0.f) ? (SecondsPerMinute / RPM) : 0.f;
	}

	/** Starting reserve ammunition for a freshly picked-up weapon. */
	int32 GetStartingReserveAmmo() const { return MagSize * ReserveMags; }

	/**
	 * Schema invariants that the CSV cannot express. Pure — call it from a spec over every
	 * row of DT_Weapons, and from a validation commandlet at import. Defined inline so this
	 * header needs no .cpp of its own (BRDataRows is a schema header, not a unit).
	 *
	 * Only STRUCTURAL invariants live here — nothing that pins a tuning value, because
	 * balance is a CSV diff and must never have to fight a C++ assert.
	 *
	 * @param OutError  human-readable reason, set only when the function returns false.
	 * @return true when the row is internally consistent.
	 */
	bool ValidateSchema(FString& OutError) const
	{
		// The delivery/speed invariant, both directions. This is the one the verifier forced.
		const bool bIsHitscan = (DamageDelivery == EBRDamageDelivery::Hitscan);
		if (bIsHitscan && ProjectileSpeed != 0.f)
		{
			OutError = TEXT("Hitscan row carries a non-zero ProjectileSpeed");
			return false;
		}
		if (!bIsHitscan && ProjectileSpeed <= 0.f)
		{
			OutError = TEXT("Projectile row carries no ProjectileSpeed");
			return false;
		}

		// A weapon that cannot hold a round cannot be fired, reloaded, or picked up sanely.
		if (MagSize <= 0)
		{
			OutError = TEXT("MagSize must be positive");
			return false;
		}
		if (ReserveMags < 0)
		{
			OutError = TEXT("ReserveMags cannot be negative");
			return false;
		}

		// No rate ceiling means the server's rate gate has nothing to compare against.
		if (RPM <= 0.f)
		{
			OutError = TEXT("RPM must be positive (it is the server's rate gate)");
			return false;
		}

		// Negatives are never a design position; they are a typo that would heal the target
		// or refund time. Zero IS legal: the Rocket's ReloadTime_s is 0 by ruling R4.
		if (DamagePerShot < 0.f || ReloadTime_s < 0.f || EquipTime_s < 0.f || SplashDamage < 0.f
			|| SplashRadius_m < 0.f)
		{
			OutError = TEXT("A damage/time/radius column is negative");
			return false;
		}

		// A headshot may never REDUCE damage; 1.0 (no bonus) is the floor, and is deliberate
		// for the AR (R2) and the Rocket (R4).
		if (HeadshotMult < 1.f)
		{
			OutError = TEXT("HeadshotMult below 1.0 would make a headshot weaker than a body shot");
			return false;
		}

		// Splash is a pair: a radius with no damage, or damage with no radius, is half a rule.
		if ((SplashRadius_m > 0.f) != (SplashDamage > 0.f))
		{
			OutError = TEXT("SplashRadius_m and SplashDamage must both be set or both be zero");
			return false;
		}

		return true;
	}
};
