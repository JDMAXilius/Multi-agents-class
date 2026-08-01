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
	 * Maximum hitscan trace length in METRES. Beyond this the shot simply misses.
	 * CSV: Range_m. Added by BP03 step 2 (1 Aug 2026).
	 *
	 * WHY IT HAD TO BE A ROW FIELD: `BRGA_WeaponFire` cannot trace without a length, and a
	 * literal in the ability body is a law-3 violation AND item four of `gas-purity` §9's
	 * self-check list. It is not in `CT_Combat` either — that table's 11 rows are all
	 * multipliers and rates. Per-weapon, so it belongs in the per-weapon row, not a curve.
	 *
	 * The SERVER uses this as a rejection bound, not just a client trace length: a client
	 * claiming a hit beyond `Range_m` is rejected (client TargetData is a CLAIM, never truth).
	 *
	 * ** VALUES ARE PROVISIONAL — curator confirmation owed. ** The schema is BP03's; the
	 * numbers are the tuning-curator's, and BP03 filed rather than invented them. They are
	 * sane starting points for a 40 m arena, not balanced figures.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Range_m = 0.f;

	/**
	 * Half-angle in DEGREES of the cone the SERVER accepts around the shooter's view direction.
	 * CSV: Spread_deg. Added by BP03 step 2 (1 Aug 2026).
	 *
	 * READ THIS BEFORE USING IT AS VISUAL SPREAD. This field is a **server validation
	 * tolerance**, not a per-shot random deviation. `BRGA_WeaponFire` traces down the camera
	 * centre and the server rejects any claimed direction outside this cone — which is what
	 * the ticket's "direction within cone of server muzzle" asks for.
	 *
	 * If per-shot random spread is added later it MUST come from a **seeded stream shared by
	 * client and server**, never `FMath::RandRange` — that is a banned API the pre-tool hook
	 * blocks outright, and here it is not merely a law: unseeded spread on a LocalPredicted
	 * ability makes client and server disagree about where the shot went on every single shot.
	 * The law and the netcode want the same thing.
	 *
	 * ** VALUES ARE PROVISIONAL — curator confirmation owed. ** See Range_m.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Spread_deg = 0.f;

	/**
	 * SOFT reference to the `UBRAbilitySet` this weapon grants on equip and revokes on stow.
	 * CSV: AbilitySet. Added by BP03 (1 Aug 2026).
	 *
	 * THIS COLUMN IS WHAT MAKES A WEAPON FIRE. Without it
	 * `BREquipmentComponent::ResolveAbilitySetForRow` had no honest answer and returned null
	 * with the log line *"the weapon equips but cannot fire"* — so every ability in
	 * `AbilitySystem/Abilities/` was unreachable code. Six abilities were written before this
	 * column existed and not one of them could be activated by a player.
	 *
	 * **`TSoftObjectPtr`, not `TSoftClassPtr`.** The gap's own comment in `BREquipmentComponent`
	 * proposed `TSoftClassPtr<UBRAbilitySet>`; that is wrong. `UBRAbilitySet` is a
	 * `UPrimaryDataAsset`, so what a weapon names is a data-asset INSTANCE (`DA_AbilitySet_AR`),
	 * not a class. A soft class pointer would resolve to the CDO and grant nothing — a failure
	 * that looks like a wiring bug and is a type error.
	 *
	 * Soft is law (`data-and-assets.md`): the set is loaded through the streamable manager at
	 * equip time, exactly like `MeshSoftPath`, so a weapon row costs nothing until equipped.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<class UBRAbilitySet> AbilitySet;

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

// Forward declarations for the SOFT references below. Declared here, in the appending block,
// rather than at the top of the file: the banner says append, never rewrite.
class UStateTree;

// ===========================================================================================
// FBRSpotterLineRow  ->  Content/Data/DT_SpotterLines.csv   (schema: sim-builder; consumer: BP11)
// ===========================================================================================

/**
 * Who hears the line.
 *
 * The enumerator names ARE the CSV's accepted cell values (same importer rule as
 * EBRWeaponFireMode: the cell text is matched against the enumerator NAME), so renaming one
 * of these breaks the 63-row table at import, not the build. Verified against the column
 * `Audience` of DT_SpotterLines.csv, whose complete value set today is {Self, Team, All}.
 */
UENUM(BlueprintType)
enum class EBRSpotterAudience : uint8
{
	/** Only the player the line is about hears it. */
	Self,
	/** The subject's team hears it. */
	Team,
	/** Everyone in the match hears it. */
	All
};

/**
 * FBRSpotterLineRow — one line of announcer/spotter VO.
 *
 * COLUMN MAP — verified against the file on disk (header row, 63 data rows):
 *
 *   RowName          -> (row key)  S01a, S01b, ... The importer consumes column 1 as the row
 *                                  NAME, so it is deliberately NOT a member.
 *   TriggerId        -> TriggerId
 *   Text             -> Text
 *   Audience         -> Audience
 *   Weight           -> Weight
 *   RepeatCooldown_s -> RepeatCooldown_s
 *
 * MANY ROWS SHARE ONE TriggerId on purpose (three lines answer `Kill.Confirmed`): the trigger
 * is the key, the row is one of its variants, and `Weight` is how the variants divide.
 *
 * SELECTION IS NOT DEFINED HERE. Whoever picks a variant must do it from a SEEDED stream
 * passed in — never FMath::RandRange — or the same match replays with different VO and the
 * spec cannot pin a line. That is a constraint on the consumer, and this comment is the only
 * place the schema can state it.
 */
USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRSpotterLineRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * The event this line answers, e.g. `Kill.Confirmed`, `Shield.Break.Self`,
	 * `Match.Phase.SuddenDeath`. CSV: TriggerId.
	 *
	 * FName, not FGameplayTag, by packet instruction. The cost is loud and worth writing down:
	 * a typo'd trigger imports CLEAN and simply never fires, where FGameplayTag would have
	 * failed at import. The consumer therefore owes a spec that asserts every distinct
	 * TriggerId in this table is one some system actually raises.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	FName TriggerId;

	/**
	 * The spoken/subtitled line. CSV: Text.
	 *
	 * A bare CSV string imports as culture-invariant text — fine for shipping English, NOT
	 * localisable. Real localisation means the cell becomes NSLOCTEXT(...) form; that is a
	 * table edit, not a schema change, which is why the type is FText today.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	FText Text;

	/** Who hears it. CSV: Audience (Self | Team | All). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	EBRSpotterAudience Audience = EBRSpotterAudience::All;

	/**
	 * Relative selection weight among the variants sharing this TriggerId. CSV: Weight.
	 *
	 * 0 is LEGAL and means "authored but never selected" — the way to mute a line without
	 * deleting it. It is not an error and must never be treated as 1.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	float Weight = 1.f;

	/** Seconds before this exact line may play again. 0 = no throttle. CSV: RepeatCooldown_s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	float RepeatCooldown_s = 0.f;

	/** Structural invariants only — nothing here pins a tuning value. */
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

// ===========================================================================================
// FBRMedalRow  ->  Content/Data/DT_Medals.csv               (schema: sim-builder; consumer: BP11)
// ===========================================================================================

/**
 * FBRMedalRow — one earnable medal.
 *
 * COLUMN MAP — verified against the file on disk (header row, 11 data rows):
 *
 *   RowName     -> (row key)  M1 .. M11. NOT a member.
 *   MedalName   -> MedalName
 *   Description -> Description
 *   TriggerId   -> TriggerId
 *
 * The medal table and the spotter table share the TriggerId VOCABULARY but not its contents:
 * DT_Medals names four triggers DT_SpotterLines does not carry a line for (Kill.First,
 * Kill.Rocket.Multi, Kill.SpreeEnder, Match.SuddenDeath.Win). That is legal — a medal may be
 * silent — but it is a fact worth a spec, not an accident to discover in playtest.
 */
USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRMedalRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Player-facing medal name (medal toast, end-of-match card). CSV: MedalName. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Medal")
	FText MedalName;

	/** One-line explanation of how it is earned. CSV: Description. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Medal")
	FText Description;

	/** The event that awards it, e.g. `Kill.Multi.Double`. CSV: TriggerId. Same FName caveat as above. */
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

// ===========================================================================================
// FBRBotConsiderationRow / FBRBotAmbitionRow  ->  Content/Data/DT_BotAmbitions.csv
// FBRBotTuningRow                             ->  Content/Data/DT_BotTuning.csv
//                                            (schema: sim-builder; consumer: BP08)
// ===========================================================================================
//
// THE ARROW POINTS ONE WAY: AI/ includes Data/. Data/ must never include AI/, so nothing
// below may name FBRBotAmbitionDef, FBRBotTierScalars or FBRBotFacts. These are the IMPORT
// shape; `UBRBotBrain::ReadAmbitionDefs` / `ReadTierScalars` are the one conversion site into
// the pure runtime shape the brain scores.
//
// COLUMN SPELLING: these two tables speak snake_case (BREACHPOINT-AI-BOTS.md's vocabulary),
// unlike DT_Weapons. Members are spelled exactly as the columns BP08 documented. Case is
// irrelevant to the importer (column lookup is an FName compare); UNDERSCORES ARE NOT, which
// is why they are preserved verbatim instead of being tidied into PascalCase.
//
// NEITHER CSV EXISTS ON DISK YET (only DT_Weapons, DT_SpotterLines, DT_Medals are there).
// These structs make BP08's loud stubs resolvable and give the table author the exact header
// row to write; they do not conjure the tables. Reported as a contract_gap.

/**
 * One consideration reference inside an ambition row's `considerations` cell.
 *
 * Mirrors FBRBotConsiderationDef (AI/BRBotBrain.h) field for field. The split exists because
 * law 3 puts the row struct here while the scored struct stays world-free in AI/.
 *
 * CSV CELL FORM — UE's parenthesised struct-array text, matched against the PROPERTY names:
 *
 *   "((Consideration=\"rocket_timer_norm\",Weight=0.8,bInvert=false),(Consideration=\"my_shield_norm\",Weight=1.0,bInvert=true))"
 *
 * LOUD NOTE FOR THE TABLE AUTHOR: BP08's ReadAmbitionDefs comment writes the third key as
 * `invert`. It is `bInvert` here — UE's bool convention, and identical to the runtime def's
 * field. Case would not have mattered (FName compare); a MISSING `b` prefix does: the token
 * simply would not resolve and the flag would import false. The two spellings must be
 * reconciled in BP08's comment, and the packet report says so out loud.
 */
USTRUCT()
struct BREACHPOINT_API FBRBotConsiderationRow
{
	GENERATED_BODY()

	/** A key of FBRBotFacts::ScalarRegistry(), e.g. `rocket_timer_norm`. Resolved in code, weighted in data. */
	UPROPERTY()
	FName Consideration;

	/** Influence in [0, 1]. 0 is the neutral element of the multiplicative combine, not "off by accident". */
	UPROPERTY()
	float Weight = 0.f;

	/** Score on (1 - fact) instead of the fact. */
	UPROPERTY()
	bool bInvert = false;
};

/**
 * FBRBotAmbitionRow — one row of `Content/Data/DT_BotAmbitions.csv`.
 *
 * COLUMN MAP — exactly the shape documented on UBRBotBrain::ReadAmbitionDefs:
 *
 *   Name               -> (row key)  ControlRocket | WinThisFight | Survive | TakeGround |
 *                                    HuntWeapon | Roam. Must match the EBRBotAmbition
 *                                    enumerator spelling; NOT a member.
 *   base_utility       -> base_utility
 *   considerations     -> considerations
 *   personality_scalar -> personality_scalar
 *   requires           -> Requires   <-- see below
 *   forbids            -> forbids
 *
 * `requires` IS A C++20 KEYWORD and cannot be an identifier in this engine's language
 * standard. The member is therefore `Requires`, capital R, which the DataTable importer still
 * matches to a column spelled `requires` because column lookup is an FName compare and FName
 * ignores case. This is the ONE deviation from BP08's documented spelling, it is forced by
 * the language rather than chosen, and it changes nothing about the CSV.
 *
 * ROW ORDER IS PART OF THE DETERMINISM CONTRACT: ties break by table order (UBRBotBrain reads
 * these into a TArray, never a TMap). Re-ordering the CSV is a behaviour change, not a tidy-up.
 */
USTRUCT()
struct BREACHPOINT_API FBRBotAmbitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The unmodified worth of this ambition before any fact is read. CSV: base_utility. */
	UPROPERTY()
	float base_utility = 0.f;

	/** Combined multiplicatively, in cell order. CSV: considerations. */
	UPROPERTY()
	TArray<FBRBotConsiderationRow> considerations;

	/**
	 * Names the DT_BotTuning column that shapes this ambition for the tier:
	 * `rocket_contest` | `push_threshold` | `cover_preference` | (empty). CSV: personality_scalar.
	 */
	UPROPERTY()
	FName personality_scalar;

	/**
	 * '|'-separated EBRBotPrecondition names, ALL of which must hold. CSV column: `requires`.
	 * A plain FString because the mask lives in AI/ and Data/ may not include it; BP08's
	 * ReadAmbitionDefs parses it and is where an unknown name must fail loudly.
	 */
	UPROPERTY()
	FString Requires;

	/** '|'-separated EBRBotPrecondition names, ANY of which forbids the ambition. CSV: forbids. */
	UPROPERTY()
	FString forbids;

	/**
	 * Structural invariants the CSV cannot express. Deliberately does NOT validate the
	 * requires/forbids names: this header cannot see EBRBotPrecondition, so that check belongs
	 * to the parser in AI/ — and it must exist there, because an unrecognised precondition
	 * silently widening a plan's legality is exactly the "probably never happens" class of bug.
	 */
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

/**
 * FBRBotTuningRow — one tier row of `Content/Data/DT_BotTuning.csv` (Recruit / Marine / Veteran).
 *
 * COLUMN MAP — the `CSV:` comments on FBRBotTierScalars (AI/BRBotBrain.h), one member each:
 *
 *   Name -> (row key, the tier name; NOT a member)
 *   reaction_ms · reaction_quantum_ms · reaction_jitter_ms · accuracy_pct · aim_error_deg ·
 *   switch_margin · commit_window_ms · commit_jitter_ms · rocket_contest · push_threshold ·
 *   cover_preference · sight_radius_m · sight_fov_deg · target_memory_s · engage_update_ms ·
 *   StateTreeSoftPath
 *
 * `engage_update_ms` is carried even though ReadTierScalars' parenthetical list omits it: the
 * `CSV:` comment on FBRBotTierScalars::EngageUpdateMs names it, and a scalars field with no
 * column would import as zero — which happens to mean "the Engage selector never re-runs".
 * A silently inert fight loop is exactly the kind of default this schema refuses.
 *
 * EVERY DEFAULT HERE IS A SENTINEL, NOT A TUNING VALUE (law 3). An unconfigured tier must be
 * visibly inert and complain; a plausible default is how an untuned bot ships.
 */
USTRUCT()
struct BREACHPOINT_API FBRBotTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	/** RULING R11's floor, in milliseconds. A law constant, never tuned — so never a CSV column. */
	static constexpr int32 ReactionFloorMs = 200;

	/**
	 * THE ONLY LEGAL WAY TO READ REACTION TIME OFF THIS ROW. R11 is enforced by making the
	 * raw column unreachable (it is private below), not by asking callers to remember.
	 *
	 * Mirrors FBRBotTierScalars::GetReactionMsClamped so that a bot's reaction is identical
	 * whether it is read from the row or from the converted scalars.
	 *
	 * @param Jitter  seeded jitter in [0, reaction_jitter_ms], drawn by the CALLER from its
	 *                own stream. Never drawn here: this struct owns no entropy and must stay
	 *                a pure function of its columns.
	 */
	int32 GetReactionMsClamped(int32 Jitter = 0) const
	{
		const int32 Raw = FMath::Max(reaction_ms, ReactionFloorMs) + FMath::Max(Jitter, 0);
		return QuantizeMs(Raw, reaction_quantum_ms);
	}

	/** Round UP to the quantum; rounding up is what keeps quantization from crossing back under the floor. */
	static int32 QuantizeMs(int32 Value, int32 Quantum)
	{
		if (Quantum <= 0)
		{
			return Value;
		}
		return ((Value + Quantum - 1) / Quantum) * Quantum;
	}

	/** Reaction delays quantize to this step, so two machines land on the same millisecond. CSV: reaction_quantum_ms. */
	UPROPERTY()
	int32 reaction_quantum_ms = 0;

	/** Seeded jitter ceiling added before quantization; 0 = none. CSV: reaction_jitter_ms. */
	UPROPERTY()
	int32 reaction_jitter_ms = 0;

	/** Fraction of shots aimed true; the complement becomes aim error. CSV: accuracy_pct. */
	UPROPERTY()
	float accuracy_pct = 0.f;

	/** Maximum cone half-angle applied BEFORE the fire ability activates. CSV: aim_error_deg. */
	UPROPERTY()
	float aim_error_deg = 0.f;

	/** Fractional score margin a challenger must beat the incumbent ambition by. CSV: switch_margin. */
	UPROPERTY()
	float switch_margin = 0.f;

	/** Minimum time an ambition is held before it may be replaced. CSV: commit_window_ms. */
	UPROPERTY()
	int32 commit_window_ms = 0;

	/** Seeded jitter on the commit window, so a squad does not think in lockstep. CSV: commit_jitter_ms. */
	UPROPERTY()
	int32 commit_jitter_ms = 0;

	/** Personality scalar, named by an ambition row's personality_scalar. CSV: rocket_contest. */
	UPROPERTY()
	float rocket_contest = 0.f;

	/** Personality scalar. CSV: push_threshold. */
	UPROPERTY()
	float push_threshold = 0.f;

	/** Personality scalar. CSV: cover_preference. */
	UPROPERTY()
	float cover_preference = 0.f;

	/** Perception radius in METRES (designer units; the perception component converts). CSV: sight_radius_m. */
	UPROPERTY()
	float sight_radius_m = 0.f;

	/** Perception cone, full angle in degrees. CSV: sight_fov_deg. */
	UPROPERTY()
	float sight_fov_deg = 0.f;

	/** Seconds a lost target stays TargetKnown. CSV: target_memory_s. */
	UPROPERTY()
	float target_memory_s = 0.f;

	/**
	 * Timer interval at which the Engage selector re-runs during a fight — a TIMER, never a
	 * tick (law 4). 0 means "never re-run", the correct inert behaviour for an unconfigured
	 * tier. CSV: engage_update_ms.
	 */
	UPROPERTY()
	int32 engage_update_ms = 0;

	/**
	 * SOFT reference to this tier's StateTree (ST_Bot or a per-tier variant). Soft is law:
	 * a hard asset ref or ConstructorHelpers here is a finding and the hook blocks the latter.
	 * CSV: StateTreeSoftPath.
	 */
	UPROPERTY()
	TSoftObjectPtr<UStateTree> StateTreeSoftPath;

	/**
	 * Structural invariants. Nothing here pins a tuning value — balance is a CSV diff and must
	 * never have to argue with a C++ assert.
	 *
	 * NOT VALIDATED ON PURPOSE: reaction_ms. A row carrying 50 is not an error, it is a row
	 * whose reaction is 200 (R11), and the accessor is what makes that true.
	 */
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
	/**
	 * CSV: reaction_ms. PRIVATE BY DESIGN — ruling R11 says no tier, no scalar combination and
	 * no future caller may produce a sub-200 ms reaction, and the only enforcement that cannot
	 * be forgotten is making the raw value unreadable. Read it through GetReactionMsClamped().
	 * The DataTable importer writes it by reflection, so privacy costs the table nothing.
	 */
	UPROPERTY()
	int32 reaction_ms = 0;
};

// ===========================================================================================
// FBRMatchRulesRow  ->  Content/Data/DT_MatchRules.csv      (schema: sim-builder; consumer: BP04)
// ===========================================================================================

/**
 * FBRMatchRulesRow — the match's tunable numbers, one row per ruleset (`Default` today).
 *
 * THIS IS THE SOURCE OF TRUTH; ABRGameMode's EditDefaultsOnly fields are the FALLBACK it uses
 * when no table is set (law 3). The handshake is BP04's existing seam, unchanged:
 *
 *     GameMode->ApplyMatchRules(Row.ScoreLimit, Row.MatchDuration_s, Row.SuddenDeathCap_s,
 *                               Row.Warmup_s, Row.RespawnDelay_s, Row.KillCreditWindow_s);
 *
 * Members are ordered to match that call, and every duration carries the `_s` suffix because
 * the suffix IS the unit contract: ApplyMatchRules takes SECONDS, and the one column that is
 * not a duration (ScoreLimit) is the one without a suffix.
 *
 * DEFAULTS ARE SENTINELS, NOT THE RULESET. Zero everywhere, so a row that failed to import is
 * loud (ValidateSchema refuses it) instead of quietly shipping a plausible match. The real
 * 25 / 480 / 60 / 15 / 5 / 5 lives in the CSV and nowhere else.
 *
 * NO PostMatch_s COLUMN. ApplyMatchRules takes six parameters and post-match duration is not
 * one of them; a seventh column would import into nothing and read as a tunable that silently
 * does not tune. ABRGameMode::PostMatchSeconds therefore stays a code-side 15 s until BP04
 * widens the seam. Reported as a contract_gap rather than papered over here.
 */
USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRMatchRulesRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Kills that end the match. CSV: ScoreLimit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	int32 ScoreLimit = 0;

	/** Length of the Live phase in seconds. CSV: MatchDuration_s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float MatchDuration_s = 0.f;

	/** Hard ceiling on the sudden-death overtime, in seconds. CSV: SuddenDeathCap_s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float SuddenDeathCap_s = 0.f;

	/** Length of the WarmUp phase in seconds. 0 is legal and means "start Live immediately". CSV: Warmup_s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float Warmup_s = 0.f;

	/** Seconds between death and the earliest legal respawn. 0 is legal (instant respawn). CSV: RespawnDelay_s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float RespawnDelay_s = 0.f;

	/** A hit older than this earns no kill credit. CSV: KillCreditWindow_s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float KillCreditWindow_s = 0.f;

	/**
	 * Structural invariants. ApplyMatchRules clamps on top of this — belt and braces, because
	 * a corrupt row must never produce a zero-second match, and the clamp lives on the server
	 * while this runs at import and in a spec.
	 *
	 * TWO EXPLICIT REFUSALS rather than silent tolerance:
	 *   - SuddenDeathCap_s == 0 is REFUSED. "No overtime" is a phase the machine should skip,
	 *     which is a rules change needing a ruling, not a zero-length phase that expires on
	 *     the frame it begins.
	 *   - KillCreditWindow_s == 0 is REFUSED. It would make every kill instigator-less, so
	 *     every death would score -1 against the victim's own team. That is a scoring exploit
	 *     one typo'd cell away, not an edge case.
	 */
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
