#pragma once

// BP91 step 3 — every row struct, ONE header, no .cpp (rework §3.2).
// Numbers live in Content/Data/*.csv; this file is schema only. A hard asset
// UPROPERTY or a ConstructorHelpers call in this header is a finding (law 3).

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h" // FRuntimeFloatCurve
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRDataRows.generated.h"

class UAnimInstance;
class UBRAbilitySet;
class USkeletalMesh;
class UStaticMesh;

/**
 * How a shot travels (rework §3.2 / §3.4: BRGA_WeaponFire reads this off the row and
 * branches — one ability serves every weapon).
 *
 * This is the pre-rework EBRDamageDelivery under its rework name. The alias below keeps
 * the two pre-rework consumers (BRGA_WeaponFire.cpp, Tests/BRCombatSpec.cpp) compiling;
 * delete the alias with the last consumer (BP94/BP98). NOTE for step 5: the serialized
 * DT_Weapons.uasset was built against the old enum name — the scripted reimport of the
 * new CSV re-types it; do not hand-open the stale asset expecting the column to survive.
 */
UENUM(BlueprintType)
enum class EBRFireMode : uint8
{
	Hitscan,
	Projectile
};

// COMPAT alias, C++-only (never a UPROPERTY type — UHT does not resolve aliases).
using EBRDamageDelivery = EBRFireMode;

/**
 * One weapon, one row. The row NAME is the weapon's identity everywhere (ini
 * StartupWeaponRow, loadout rows, pickups); renaming a row is a breaking change.
 */
USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRWeaponRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- canonical numbers (BP91 schema) -------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float DamagePerShot = 0.f;

	// Server rate gate: max sustained fire, rounds per minute.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float RPM = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 MagSize = 0;

	// Max reserve ROUNDS carried outside the magazine.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 ReserveMax = 0;

	// Hip-fire cone half-angle, degrees. Also the server's direction-claim tolerance.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float SpreadDegrees = 0.f;

	// Hard range ceiling, metres. Beyond it a hitscan claim is rejected outright.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float RangeMax = 0.f;

	// ---- shaping -------------------------------------------------------------------

	// Damage multiplier over normalized distance (0 = muzzle, 1 = RangeMax).
	// An EMPTY curve means "no falloff" — the exec calc treats it as constant 1.0,
	// which BRDamageExecCalc (BP94) must implement and pin.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FRuntimeFloatCurve DamageFalloff;

	// Per-body-section damage multipliers keyed by physics-asset section name
	// ("head", "torso"...). A section absent from the map is 1.0. The weak point is
	// DERIVED (any entry > 1.0), never declared (rework §3.4).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TMap<FName, float> BodySectionMods;

	// COMPAT NAME, canonical field: how the shot travels. Pre-rework consumers read
	// `DamageDelivery`; rename to FireMode with the last of them (BP94/BP98).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EBRFireMode DamageDelivery = EBRFireMode::Hitscan;

	// ---- soft refs ONLY (law 3) ----------------------------------------------------

	// The HELD mesh, skeletal — reload/bolt/recoil animate on this skeleton.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<USkeletalMesh> HeldMeshSoftPath;

	// The weapon's ability set (Fire/Reload/Swap specs), granted on equip.
	// DECISION (BP91 Log): TSoftObjectPtr, not the ticket's TSoftClassPtr — UBRAbilitySet
	// is a UPrimaryDataAsset (rework §3.4 keeps it a DataAsset) and the live consumer
	// (BREquipmentComponent::ResolveAbilitySetForRow) loads an INSTANCE. BP93 owns the
	// class; if it re-types the set as a class-per-weapon, this column moves with it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<UBRAbilitySet> AbilitySet;

	// ---- COMPAT fields — pre-rework consumers outside Data/ still compile against
	// ---- these. Each is delete-with-last-consumer; consumers named per field. ------

	// COMPAT: UI/BRHUDDirector.cpp (weapon name lines).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	FText DisplayName;

	// COMPAT: GetStartingReserveAmmo below + Tests/BRCombatSpec.cpp (reserve = MagSize
	// x ReserveMags). Superseded by ReserveMax; BP98's ammo model migrates the spec.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	int32 ReserveMags = 0;

	// COMPAT: AbilitySystem/Abilities/BRGA_WeaponUtility.cpp (montage fallback timing).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	float ReloadTime_s = 0.f;

	// COMPAT: Tests/BRCombatSpec.cpp TTK pins. BP94's exec calc replaces this with
	// BodySectionMods + CT_Combat's Damage.Headshot.Multiplier.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	float HeadshotMult = 1.f;

	// COMPAT: ValidateSchema + Tests/BRCombatSpec.cpp; BP101's projectile row shape
	// decides its successor. Metres per second.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	float ProjectileSpeed = 0.f;

	// COMPAT: Tests/BRCombatSpec.cpp splash pins (both-or-neither rule below).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	float SplashRadius_m = 0.f;

	// COMPAT: Tests/BRCombatSpec.cpp splash pins.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	float SplashDamage = 0.f;

	// COMPAT: BRGA_WeaponUtility.cpp + BRCombatSpec.cpp R3 swap-TTK pins.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	float EquipTime_s = 0.f;

	// COMPAT: BRGA_WeaponFire.cpp range validation. Superseded by RangeMax (metres).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	float Range_m = 0.f;

	// COMPAT: BRGA_WeaponFire.cpp direction-claim validation. Superseded by SpreadDegrees.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	float Spread_deg = 0.f;

	// COMPAT: Equipment/BRWeaponPickup.cpp — the world-pickup mesh, static.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	TSoftObjectPtr<UStaticMesh> MeshSoftPath;

	// COMPAT: Equipment/BREquipmentComponent.cpp anim-layer switch. Deliberately
	// OPTIONAL (see ValidateSchema note); BP99's anim seam decides its successor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	TSoftClassPtr<UAnimInstance> FirstPersonAnimBP;

	// COMPAT: Equipment/BREquipmentComponent.cpp anim-layer switch.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	TSoftClassPtr<UAnimInstance> ThirdPersonAnimBP;

	// COMPAT: AbilitySystem fire cue routing; BP94's cue rework re-homes it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Compat")
	FGameplayTag FireCueTag;

	// ---- row algebra (pinned by Tests/BRCombatSpec.cpp — behavior changes move the
	// ---- pin LOUDLY in the same packet) --------------------------------------------

	static constexpr float CmPerMetre = 100.f;

	static constexpr float SecondsPerMinute = 60.f;

	float GetSplashRadiusCm() const { return SplashRadius_m * CmPerMetre; }

	float GetProjectileSpeedCmPerSecond() const { return ProjectileSpeed * CmPerMetre; }

	float GetMinShotIntervalSeconds() const
	{
		return (RPM > 0.f) ? (SecondsPerMinute / RPM) : 0.f;
	}

	int32 GetStartingReserveAmmo() const { return MagSize * ReserveMags; }

	bool ValidateSchema(FString& OutError) const
	{
		const bool bIsHitscan = (DamageDelivery == EBRFireMode::Hitscan);
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

		if (RPM <= 0.f)
		{
			OutError = TEXT("RPM must be positive (it is the server's rate gate)");
			return false;
		}

		if (DamagePerShot < 0.f || ReloadTime_s < 0.f || EquipTime_s < 0.f || SplashDamage < 0.f
			|| SplashRadius_m < 0.f)
		{
			OutError = TEXT("A damage/time/radius column is negative");
			return false;
		}

		if (HeadshotMult < 1.f)
		{
			OutError = TEXT("HeadshotMult below 1.0 would make a headshot weaker than a body shot");
			return false;
		}

		if ((SplashRadius_m > 0.f) != (SplashDamage > 0.f))
		{
			OutError = TEXT("SplashRadius_m and SplashDamage must both be set or both be zero");
			return false;
		}

		// A weapon with no held mesh equips, fires and kills while being invisible in the
		// hand — the worst kind of working, because nothing in any log mentions it.
		if (HeldMeshSoftPath.IsNull())
		{
			OutError = TEXT("HeldMeshSoftPath is unset — this weapon would be invisible in the hand");
			return false;
		}

		if (ThirdPersonAnimBP.IsNull())
		{
			OutError = TEXT("ThirdPersonAnimBP unset — everyone else would watch this pawn hold the weapon in an unarmed pose");
			return false;
		}

		// FirstPersonAnimBP is deliberately OPTIONAL, and every shipped row leaves it empty.
		// The template's first-person weapon layers (ABP_FP_Weapon, ABP_FP_Pistol) cast to
		// BP_FirstPersonCharacter_C and read its "First Person Camera"; on a BRCharacter that
		// cast fails and the anim BP logs "Accessed None" EVERY FRAME. Until an FP layer
		// exists that does not hard-cast to a pawn class we do not use, an unset column means
		// "keep the arms on whatever they already wear", which is what the equipment
		// component's fallback does. The third-person layers carry no such coupling.

		return true;
	}
};

/**
 * One named loadout: which weapons fill the slots on spawn. Row-name keys into
 * DT_Weapons — the row IS the weapon's identity — plus one count. No asset refs
 * here at all: everything an asset touches hangs off the weapon row.
 */
USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRLoadoutRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	FName PrimaryWeaponRow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	FName SecondaryWeaponRow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	int32 GrenadeCount = 0;

	bool ValidateSchema(FString& OutError) const
	{
		if (PrimaryWeaponRow.IsNone())
		{
			OutError = TEXT("PrimaryWeaponRow is empty: this loadout spawns an unarmed player");
			return false;
		}
		if (GrenadeCount < 0)
		{
			OutError = TEXT("GrenadeCount is negative");
			return false;
		}
		return true;
	}
};

/**
 * Match rules, numbers only. KEPT under its pre-rework name (the ticket says
 * FBRMatchRules): Content/Data/DT_MatchRules.uasset is serialized against
 * "FBRMatchRulesRow" and renaming the struct orphans the table with zero code
 * consumers to show for it. Rename when BP96 (Match rework) touches the table.
 */
USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRMatchRulesRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	int32 ScoreLimit = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float MatchDuration_s = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float SuddenDeathCap_s = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float Warmup_s = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float RespawnDelay_s = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float KillCreditWindow_s = 0.f;

	bool ValidateSchema(FString& OutError) const
	{
		if (ScoreLimit <= 0)
		{
			OutError = TEXT("ScoreLimit must be positive (0 would end the match at kickoff)");
			return false;
		}
		if (MatchDuration_s <= 0.f)
		{
			OutError = TEXT("MatchDuration_s must be positive");
			return false;
		}
		if (SuddenDeathCap_s <= 0.f)
		{
			OutError = TEXT("SuddenDeathCap_s must be positive ('no overtime' is a ruling, not a zero)");
			return false;
		}
		if (KillCreditWindow_s <= 0.f)
		{
			OutError = TEXT("KillCreditWindow_s must be positive (0 makes every kill instigator-less)");
			return false;
		}
		if (Warmup_s < 0.f || RespawnDelay_s < 0.f)
		{
			OutError = TEXT("Warmup_s / RespawnDelay_s cannot be negative");
			return false;
		}
		return true;
	}
};

// =====================================================================================
// Rows below serve systems the rework declares OUT OF SCOPE (UI spotter/medals, AI
// brain — rework doc header) and their DT_*.uasset tables exist in Content/Data/.
// Kept verbatim; each moves only when its owning system's packet moves it.
// =====================================================================================

class UStateTree;

UENUM(BlueprintType)
enum class EBRSpotterAudience : uint8
{
	Self,
	Team,
	All
};

USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRSpotterLineRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	FName TriggerId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	FText Text;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	EBRSpotterAudience Audience = EBRSpotterAudience::All;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	float Weight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	float RepeatCooldown_s = 0.f;

	bool ValidateSchema(FString& OutError) const
	{
		if (TriggerId.IsNone())
		{
			OutError = TEXT("TriggerId is empty: this line can never be selected");
			return false;
		}
		if (Text.IsEmpty())
		{
			OutError = TEXT("Text is empty: a silent line is a missing line, not a design position");
			return false;
		}
		if (Weight < 0.f)
		{
			OutError = TEXT("Weight is negative (0 is the legal way to mute a line)");
			return false;
		}
		if (RepeatCooldown_s < 0.f)
		{
			OutError = TEXT("RepeatCooldown_s is negative");
			return false;
		}
		return true;
	}
};

USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRMedalRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Medal")
	FText MedalName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Medal")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Medal")
	FName TriggerId;

	bool ValidateSchema(FString& OutError) const
	{
		if (TriggerId.IsNone())
		{
			OutError = TEXT("TriggerId is empty: this medal can never be awarded");
			return false;
		}
		if (MedalName.IsEmpty())
		{
			OutError = TEXT("MedalName is empty");
			return false;
		}
		return true;
	}
};

USTRUCT()
struct BREACHPOINT_API FBRBotConsiderationRow
{
	GENERATED_BODY()

	UPROPERTY()
	FName Consideration;

	UPROPERTY()
	float Weight = 0.f;

	UPROPERTY()
	bool bInvert = false;
};

USTRUCT()
struct BREACHPOINT_API FBRBotAmbitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY()
	float base_utility = 0.f;

	UPROPERTY()
	TArray<FBRBotConsiderationRow> considerations;

	UPROPERTY()
	FName personality_scalar;

	UPROPERTY()
	FString Requires;

	UPROPERTY()
	FString forbids;

	bool ValidateSchema(FString& OutError) const
	{
		if (base_utility < 0.f)
		{
			OutError = TEXT("base_utility is negative: an ambition cannot be worth less than nothing");
			return false;
		}
		for (const FBRBotConsiderationRow& Consideration : considerations)
		{
			if (Consideration.Consideration.IsNone())
			{
				OutError = TEXT("A consideration names no fact");
				return false;
			}
			if (Consideration.Weight < 0.f || Consideration.Weight > 1.f)
			{
				OutError = TEXT("A consideration weight is outside [0, 1] (weight is INFLUENCE, not a multiplier)");
				return false;
			}
		}
		return true;
	}
};

USTRUCT()
struct BREACHPOINT_API FBRBotTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	static constexpr int32 ReactionFloorMs = 200;

	int32 GetReactionMsClamped(int32 Jitter = 0) const
	{
		const int32 Raw = FMath::Max(reaction_ms, ReactionFloorMs) + FMath::Max(Jitter, 0);
		return QuantizeMs(Raw, reaction_quantum_ms);
	}

	static int32 QuantizeMs(int32 Value, int32 Quantum)
	{
		if (Quantum <= 0)
		{
			return Value;
		}
		return ((Value + Quantum - 1) / Quantum) * Quantum;
	}

	UPROPERTY()
	int32 reaction_quantum_ms = 0;

	UPROPERTY()
	int32 reaction_jitter_ms = 0;

	UPROPERTY()
	float accuracy_pct = 0.f;

	UPROPERTY()
	float aim_error_deg = 0.f;

	UPROPERTY()
	float switch_margin = 0.f;

	UPROPERTY()
	int32 commit_window_ms = 0;

	UPROPERTY()
	int32 commit_jitter_ms = 0;

	UPROPERTY()
	float rocket_contest = 0.f;

	UPROPERTY()
	float push_threshold = 0.f;

	UPROPERTY()
	float cover_preference = 0.f;

	UPROPERTY()
	float sight_radius_m = 0.f;

	UPROPERTY()
	float sight_fov_deg = 0.f;

	UPROPERTY()
	float target_memory_s = 0.f;

	UPROPERTY()
	int32 engage_update_ms = 0;

	UPROPERTY()
	TSoftObjectPtr<UStateTree> StateTreeSoftPath;

	bool ValidateSchema(FString& OutError) const
	{
		if (reaction_quantum_ms < 0 || reaction_jitter_ms < 0 || commit_window_ms < 0
			|| commit_jitter_ms < 0 || engage_update_ms < 0)
		{
			OutError = TEXT("A millisecond column is negative");
			return false;
		}
		if (accuracy_pct < 0.f || accuracy_pct > 1.f)
		{
			OutError = TEXT("accuracy_pct is outside [0, 1] (it is a fraction, not a percentage)");
			return false;
		}
		if (aim_error_deg < 0.f || aim_error_deg > 180.f)
		{
			OutError = TEXT("aim_error_deg is outside [0, 180]");
			return false;
		}
		if (switch_margin < 0.f)
		{
			OutError = TEXT("switch_margin is negative: a challenger would win by losing");
			return false;
		}
		if (sight_radius_m < 0.f || sight_fov_deg < 0.f || sight_fov_deg > 360.f)
		{
			OutError = TEXT("sight_radius_m / sight_fov_deg are outside their legal range");
			return false;
		}
		if (target_memory_s < 0.f)
		{
			OutError = TEXT("target_memory_s is negative");
			return false;
		}
		return true;
	}

private:
	UPROPERTY()
	int32 reaction_ms = 0;
};
