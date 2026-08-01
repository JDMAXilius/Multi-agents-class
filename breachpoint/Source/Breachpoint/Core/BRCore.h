// Breachpoint. Log channels and collision channel aliases.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

#include "BRCore.generated.h"

/**
 * BRCore — the project-wide log channels, collision channel aliases, and the GAS stage gate.
 *
 * Specification: BREACHPOINT-ARCHITECTURE.md §3.1. The first two are one file on purpose
 * (v2 necessity audit: "BRLogChannels + BRCollision -> MERGED, same audience, one include").
 * The stage gate joins them for the same reason and no other: EVERY discipline folder has to be
 * able to ask "is my stage on?" without including a gameplay header, and BRCore.h is already the
 * one header every one of them includes.
 */

// ---------------------------------------------------------------------------
// Log channels — one per discipline, so a filtered log reads as one subsystem.
// Defined in BRCore.cpp. The template's own Logbreachpoint is untouched and unused by us.
//
// EXTENSION RULE (ruling R24, no new ruling needed per channel): a §3 discipline folder
// that needs to speak gets LogBR<Folder>, added HERE by the packet that first needs it,
// under an exact-file owner_path grant on BRCore.h/.cpp (the same device R23 defines for
// BRGameplayTags.h/.cpp). §3.1's five channels cover twelve folders; that is an oversight,
// not a design position. Add on demand, never speculatively.
// ---------------------------------------------------------------------------
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRCombat, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRNet, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRAI, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBROnline, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRUI, Log, All);

/**
 * LogBRInput — the Input/ folder's channel (ruling R24; added by BP01 step 4).
 *
 * It carries the five-arrow flow of §3.2 end to end: IMC -> UInputAction ->
 * UBRInputComponent -> InputTag -> ABRPlayerController::AbilityInputTag*(Tag) -> (BP02) ASC.
 * Without it, step 3a could only reach for development-only ensureMsgf, and BP01's Done-when
 * box 4 ("native input flows ... log-proven") had no channel on which to be proven.
 *
 * Verbosity policy for everything that logs here (law 4's spirit — no per-frame spam):
 *   Log         — one-shot seams: bind setup, mapping-context add, first Move/Look received,
 *                 and each ability tag's press/release EDGE.
 *   Verbose     — repeat traffic: every Triggered while a key is held.
 *   VeryVerbose — per-frame axis values.
 * A verifier proving box 4 runs `log LogBRInput Verbose` (or VeryVerbose) in the console; the
 * channel's compiled-in level is All, so nothing is stripped from a development build.
 */
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRInput, Log, All);

/**
 * Collision channel aliases.
 *
 * These MUST agree with [/Script/Engine.CollisionProfile] in Config/DefaultEngine.ini.
 * A mismatch is silent — nothing fails to compile, things simply stop colliding — so the
 * ini and this block are edited together, in the same commit, or not at all.
 *
 *   alias                       engine channel         ini Name         kind
 *   -------------------------   --------------------   --------------   ---------------
 *   BRCollision::Projectile     ECC_GameTraceChannel1  "Projectile"     object (template)
 *   BRCollision::WeaponTrace    ECC_GameTraceChannel2  "BRWeapon"       trace
 *   BRCollision::MeleeTrace     ECC_GameTraceChannel3  "BRMelee"        trace
 *   BRCollision::GrappleTrace   ECC_GameTraceChannel4  "BRGrapple"      trace
 *
 * Channel 1 is the First Person template's own object channel and predates this project;
 * it is aliased here rather than redefined so nobody reuses the slot. Channels 2-4 are ours.
 */
/**
 * Unit conversions. Structural constants — nobody balances these.
 *
 * `MetresToUU` lives HERE, once, because five ability/weapon .cpp files each defined it in an
 * anonymous namespace and UE's UNITY BUILD concatenates translation units — five anonymous
 * namespaces become one, and the definitions collide (C2374/C2086). Correct C++ per file,
 * broken in aggregate, and invisible in a non-unity build.
 *
 * Data is authored in METRES (`data-and-assets.md`); the engine works in centimetres.
 */
namespace BRUnits
{
	inline constexpr float MetresToUU = 100.f;
}

namespace BRCollision
{
	/** Object channel for projectile actors (grenades, rockets). ini Name="Projectile". */
	inline constexpr ECollisionChannel Projectile = ECC_GameTraceChannel1;

	/** Hitscan weapon fire trace (BRGA_WeaponFire, client trace + server revalidation). ini Name="BRWeapon". */
	inline constexpr ECollisionChannel WeaponTrace = ECC_GameTraceChannel2;

	/** Melee notify-window trace (BRGA_Melee). ini Name="BRMelee". */
	inline constexpr ECollisionChannel MeleeTrace = ECC_GameTraceChannel3;

	/** Grapple target trace / surface classification (BRGA_Grapple). ini Name="BRGrapple". */
	inline constexpr ECollisionChannel GrappleTrace = ECC_GameTraceChannel4;
}

// ---------------------------------------------------------------------------
// THE GAS STAGE GATE — docs/GAS-INTEGRATION-ROADMAP.md
// ---------------------------------------------------------------------------

/**
 * EBRGasStage — how far the GAS integration is switched on, right now, in this process.
 *
 * THE RULE THIS ENUM EXISTS TO ENFORCE (GAS-INTEGRATION-ROADMAP.md, "The one rule"):
 * *"Every stage is a switch that defaults OFF, flips ON alone, and reverts in one edit."*
 * The template works and our framework does not, yet; so the integration is ordered by one
 * question — *if this step is wrong, can the founder still play?* — and every step must be
 * revertible from an ini, because a rebuild needs the editor closed (R36) and the founder is
 * mid-playtest.
 *
 * ORDERED, AND THE ORDER IS BLAST RADIUS, NOT TICKET NUMBER. The values are contiguous and
 * ascending precisely so `BRGas::IsStageEnabled(Required)` can be a single `>=`: a call site
 * asks "have we reached at least `Granting`?", never "is the stage exactly N?". Reordering these
 * enumerators silently re-orders the integration; adding one in the middle is a design decision,
 * not a code edit, and belongs in the roadmap first.
 *
 * Each stage's exit criterion, log line and known traps are in the roadmap — they are NOT
 * duplicated here, because two copies of a checklist is one copy nobody updates.
 */
UENUM()
enum class EBRGasStage : uint8
{
	/** Nothing GAS-side runs. The template, exactly as it plays today. THE DEFAULT. */
	Off = 0,

	/** ASC init + GE_InitStats. Numbers exist; nothing reads them. */
	AttributesOnly = 1,

	/** Ability sets granted. Abilities exist; no input reaches them. */
	Granting = 2,

	/** Ability input tags reach the ASC. Abilities activate; most do nothing. */
	InputRouted = 3,

	/** The first ability with a visible effect — and the only one that touches the CMC. */
	Sprint = 4,

	/** Fire, reload, swap. The first stage that can kill the player. */
	Weapons = 5,

	/** Melee, grenade, grapple. Grapple is the only ability that moves the player. */
	FullSandbox = 6,

	/** FX last, because it is the only layer that cannot break gameplay. */
	Cues = 7
};

/**
 * BRGas — the single read path for the stage gate.
 *
 * WHERE THE VALUE COMES FROM: `GasStage` in `[/Script/Breachpoint.BRCharacter]` of
 * `Config/DefaultGame.ini`, loaded by UE into `ABRCharacter`'s C++ CDO because that class is
 * `config = Game` and the property carries `Config` — the same mechanism, on the same class, in
 * the same ini section as `bBindNativeVerbsFromInputConfig` (docs/PHASE2-RELAYER.md step 2).
 * `GetStage()` reads that CDO once per process and caches it.
 *
 * WHY THE CDO AND NOT AN INSTANCE, stated once because it is the whole safety argument: reading
 * `GetDefault<ABRCharacter>()` reads the value UE loaded FROM THE INI, which a Blueprint child's
 * saved details panel cannot override. `PHASE2-RELAYER.md` step 1 documents the opposite case —
 * "a Blueprint's *serialised* property beats the ini-loaded C++ CDO" — and that is a hole the
 * founder must not be able to fall into on the one switch whose whole purpose is reverting a
 * regression without a rebuild. The property is therefore `UPROPERTY(Config)` with **no** edit
 * specifier: the ini is not the preferred place to set it, it is the ONLY place.
 *
 * ONCE PER PROCESS, NOT PER PIE. Config is read at CDO construction (PHASE2-RELAYER step 1 says
 * so and proves it), so changing the ini takes an editor restart, not a rebuild. That is the
 * distinction the roadmap cares about: a restart costs the founder a minute, a rebuild costs
 * them their editor session.
 */
namespace BRGas
{
	/**
	 * The configured stage. Resolved and logged ONCE per process, at Warning, in the shape
	 * `BRGas: stage = AttributesOnly (Config/DefaultGame.ini)`.
	 *
	 * The log is not decoration. A silent gate is indistinguishable from a broken feature, and
	 * this project has already lost a day to exactly that confusion — so the line prints even when
	 * the stage is `Off` and nothing else in the GAS layer will say a word all session. It is
	 * emitted from an `EndOfEngineInit` hook in BRCore.cpp so it does not depend on any call site
	 * ever asking.
	 */
	BREACHPOINT_API EBRGasStage GetStage();

	/**
	 * "Have we reached at least this stage?" — the ONE question every gated call site asks.
	 *
	 * Intended shape, and it should read like an early return because it is one:
	 * @code
	 *   if (!BRGas::IsStageEnabled(EBRGasStage::Granting)) { return; }
	 * @endcode
	 *
	 * `IsStageEnabled(EBRGasStage::Off)` is always true and is therefore meaningless as a guard —
	 * `Off` is the absence of a stage, not a stage you can be "at least" at.
	 */
	BREACHPOINT_API bool IsStageEnabled(EBRGasStage Required);

	/** The enumerator's name, for logs. A switch, not reflection — this runs before the log line. */
	BREACHPOINT_API const TCHAR* ToString(EBRGasStage Stage);
}
