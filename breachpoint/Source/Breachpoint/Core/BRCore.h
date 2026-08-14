#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GenericTeamAgentInterface.h"

// BP91: log channels · collision channel aliases · team attitude. That is all of Core.

BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRCombat, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRNet, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRAI, Log, All);

// COMPAT: pre-rework AbilitySystem/ and Match/ code still logs here; delete when
// BP93/BP95 rewrite those consumers onto LogBRCombat/LogBRNet.
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRAbility, Log, All);

namespace BRUnits
{
	inline constexpr float MetresToUU = 100.f;
}

namespace BRCollision
{
	// Mirrors Config/DefaultEngine.ini [/Script/Engine.CollisionProfile] — verbatim:
	//   ECC_GameTraceChannel1  Name="Projectile"  (object channel)
	//   ECC_GameTraceChannel2  Name="BRWeapon"    (trace channel)
	//   ECC_GameTraceChannel3  Name="BRMelee"     (trace channel)
	//   ECC_GameTraceChannel4  Name="BRGrapple"   (trace channel)
	inline constexpr ECollisionChannel Projectile = ECC_GameTraceChannel1;
	inline constexpr ECollisionChannel WeaponTrace = ECC_GameTraceChannel2;
	inline constexpr ECollisionChannel MeleeTrace = ECC_GameTraceChannel3;
	inline constexpr ECollisionChannel GrappleTrace = ECC_GameTraceChannel4;
}

class AActor;

namespace BRTeams
{
	// The ENTIRE team system: two fixed sides need an id (IGenericTeamAgentInterface,
	// which AI perception reads for free) and this one attitude function.
	// Null or team-less actor on either side -> Neutral, never Hostile.
	BREACHPOINT_API ETeamAttitude::Type GetAttitude(const AActor* A, const AActor* B);
}
