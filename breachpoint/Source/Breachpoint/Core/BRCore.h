#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

// Shared by the ability base, the ASC and the PlayerState: the grant, the input match and the
// activation are three points on one chain and reading them apart is what makes it debuggable.
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRAbility, Log, All);

namespace BRUnits
{
	inline constexpr float MetresToUU = 100.f;
}

namespace BRCollision
{
	inline constexpr ECollisionChannel Projectile = ECC_GameTraceChannel1;

	inline constexpr ECollisionChannel WeaponTrace = ECC_GameTraceChannel2;

	inline constexpr ECollisionChannel MeleeTrace = ECC_GameTraceChannel3;

	inline constexpr ECollisionChannel GrappleTrace = ECC_GameTraceChannel4;
}
