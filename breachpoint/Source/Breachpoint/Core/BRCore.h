// Breachpoint. Log channels and collision channel aliases.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/**
 * BRCore — the project-wide log channels and collision channel aliases.
 *
 * Specification: BREACHPOINT-ARCHITECTURE.md §3.1. The two are one file on purpose
 * (v2 necessity audit: "BRLogChannels + BRCollision -> MERGED, same audience, one include").
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
