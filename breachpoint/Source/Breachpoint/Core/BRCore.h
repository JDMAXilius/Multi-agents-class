#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

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
