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
// ---------------------------------------------------------------------------
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRCombat, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRNet, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRAI, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBROnline, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRUI, Log, All);

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
