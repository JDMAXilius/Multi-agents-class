// Breachpoint. Log channels and collision channel aliases.
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

#include "BRCore.generated.h"

BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRCombat, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRNet, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRAI, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBROnline, Log, All);
BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRUI, Log, All);

BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRInput, Log, All);

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

UENUM()
enum class EBRGasStage : uint8
{
	Off = 0,

	AttributesOnly = 1,

	Granting = 2,

	InputRouted = 3,

	Sprint = 4,

	Weapons = 5,

	FullSandbox = 6,

	Cues = 7
};

namespace BRGas
{
	BREACHPOINT_API EBRGasStage GetStage();

	BREACHPOINT_API bool IsStageEnabled(EBRGasStage Required);

	BREACHPOINT_API const TCHAR* ToString(EBRGasStage Stage);
}
