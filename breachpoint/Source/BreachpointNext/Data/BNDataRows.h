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
/**
 * HOW GOOD IS THIS BOT — the tier, and the only thing a tier is: a row of numbers that already
 * existed as scattered config on the controller. Halo Infinite's four tiers are not one skill
 * slider; they move reaction, aim, awareness and movement independently, and a Recruit that
 * simply "aims worse" reads as a broken Spartan rather than a rookie.
 *
 * Keyed by the tier's own name (Recruit / Marine / ODST / Spartan) so the table needs no enum,
 * and UBNBotController::DefaultTuning mirrors these rows in C++ — the table OVERRIDES, never
 * duplicates, exactly as the ambition rows do.
 */
USTRUCT(BlueprintType)
struct FBNBotTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The window between seeing a target and being ALLOWED to shoot it. The single most
	 *  felt number in the whole table: it is the beat a player gets to react first. */
	UPROPERTY(EditAnywhere, Category = "Reaction")
	float ReactionSecondsMin = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Reaction")
	float ReactionSecondsMax = 0.45f;

	/** Half-angle of the cone the aim is jittered inside, re-drawn every ReaimSeconds. Zero is
	 *  hitscan-perfect and is deliberately reachable — that is what the top tier is for. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	float AimErrorDegrees = 2.5f;

	/** How often the jitter is re-drawn. A LONGER redraw is easier to fight, not harder: the
	 *  error holds still long enough to strafe out of, where a fast redraw averages onto you. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	float ReaimSeconds = 0.5f;

	// ---- awareness: how far and how wide this bot notices anything ----
	//
	// THE FOUNDER'S NUMBERS, not the engine's defaults, and they are a MAP fact before they are a
	// difficulty one: at 2500/3000 a bot could see most of BR_Arena01 from where it stood, so
	// every bot always had a target, the tree never left Engage, Search was unreachable and
	// nobody ever roamed. 1200/1500 is roughly one arena segment — bots lose each other around
	// corners, which is what makes hunting a behaviour instead of a dead branch.
	//
	// Every tier is scaled around THIS, not around the old defaults. A tier that sees the whole
	// map is not a harder bot, it is the bug that was already fixed once.
	UPROPERTY(EditAnywhere, Category = "Sight")
	float SightRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Sight")
	float LoseSightRadius = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Sight")
	float PeripheralVisionAngleDegrees = 70.f;

	/** How far a NOISE reaches this bot (R10). Deliberately longer than sight: you hear a
	 *  firefight through a wall you cannot see through, and that asymmetry is most of what makes
	 *  a level feel occupied. Zero deafens the tier — which is what makes Recruit a tier a
	 *  player can flank. */
	UPROPERTY(EditAnywhere, Category = "Sight")
	float HearingRange = 2200.f;

	/** How often this bot may leave the ground. A Recruit that never jumps is readable; a
	 *  Spartan that jukes every second duel is not. */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float JumpCooldownSeconds = 1.5f;

	/** Seconds between sidesteps while shooting, and how often a sidestep becomes a jump.
	 *  Zero juke disables it — the lowest tier stands and trades, which is what makes it the
	 *  tier a new player can beat. */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float StrafeIntervalSeconds = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	int32 JukeEveryNthStep = 3;

	/** Does this tier get out of the way of a grenade (R10.4)? Halo's shape: the low tiers are
	 *  the ones you can catch with one, and a Recruit that dodges is not a Recruit. */
	UPROPERTY(EditAnywhere, Category = "Movement")
	bool bEvadesBlasts = true;
};

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

/** One ambition's tuning (R6). Row names are the EBNBotAmbition literals: Fight, Survive, Roam.
 *  The C++ fallback values live in UBNBotBrain::DefaultRow — the table OVERRIDES, never
 *  duplicates, and a missing table means the defaults drive after one warning. */
USTRUCT(BlueprintType)
struct FBNBotAmbitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ambition")
	float BaseUtility = 1.f;

	UPROPERTY(EditAnywhere, Category = "Ambition")
	float HealthWeight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Ambition")
	float TargetWeight = 1.f;

	/** Named slot: no coded evaluator reads it this wave (considerations are health and target). */
	UPROPERTY(EditAnywhere, Category = "Ambition")
	float DistanceWeight = 0.f;

	/** Hysteresis window: the chosen ambition holds this long — commitment is legibility. */
	UPROPERTY(EditAnywhere, Category = "Ambition")
	float CommitSeconds = 3.f;

	/** Meaningful on the SURVIVE row only: health below this fraction interrupts ANY commit,
	 *  immediately. The critic proved a ratio-based interrupt was unreachable with real weights —
	 *  a bot at 5% health stood firing through its whole Fight window. A threshold is the roadmap's
	 *  own sentence ("health crossing the Survive threshold interrupts anything") as one number. */
	UPROPERTY(EditAnywhere, Category = "Ambition", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InterruptBelowHealthNorm = 0.f;
};
