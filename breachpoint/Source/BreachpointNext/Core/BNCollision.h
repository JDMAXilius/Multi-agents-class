#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/**
 * The project's dedicated trace channels, mirroring Config/DefaultEngine.ini
 * [/Script/Engine.CollisionProfile] verbatim:
 *
 *   ECC_GameTraceChannel1  Name="Projectile"  (object channel)
 *   ECC_GameTraceChannel2  Name="BRWeapon"    (trace channel)
 *   ECC_GameTraceChannel3  Name="BRMelee"     (trace channel)
 *   ECC_GameTraceChannel4  Name="BRGrapple"   (trace channel)
 *
 * The `BR` in the ini names is historical — these are the project's channels, and the BN module
 * uses the same ones rather than burning new slots on a duplicate set.
 *
 * WHY a dedicated channel and not ECC_Visibility: no character collision profile responds to
 * Visibility. `Pawn` (the capsule), `CharacterMesh` (the body) and `Ragdoll` (the corpse) all set
 * Visibility to Ignore in BaseEngine.ini, deliberately, so that camera and line-of-sight queries
 * pass through people. A weapon trace on Visibility therefore passes through every player in the
 * game and hits the wall behind them — silently, with a perfectly valid FHitResult.
 */
namespace BNCollision
{
	inline constexpr ECollisionChannel Projectile = ECC_GameTraceChannel1;
	inline constexpr ECollisionChannel WeaponTrace = ECC_GameTraceChannel2;
	inline constexpr ECollisionChannel MeleeTrace = ECC_GameTraceChannel3;
	inline constexpr ECollisionChannel GrappleTrace = ECC_GameTraceChannel4;
}
