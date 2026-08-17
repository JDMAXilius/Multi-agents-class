#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "BRCombatCurves.generated.h"

class UCurveTable;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "BR Combat Curve Config"))
class BREACHPOINT_API UBRCombatCurveConfig : public UObject
{
	GENERATED_BODY()

public:
	UBRCombatCurveConfig();

	UPROPERTY(Config, EditDefaultsOnly, Category = "Breachpoint|Combat")
	TSoftObjectPtr<UCurveTable> CombatCurveTable;
};

namespace BRCombatCurves
{
	namespace Names
	{
		inline const TCHAR* DamageMultiplierSuffix = TEXT(".Multiplier");

		inline const FName MovementBaseSpeed = FName(TEXT("Movement.BaseSpeed"));

		inline const FName MovementSprintSpeedMultiplier = FName(TEXT("Movement.Sprint.SpeedMultiplier"));

		inline const FName ShieldsRegenRatePerSecond = FName(TEXT("Shields.Regen.RatePerSecond"));

		inline const FName ShieldsRegenDelaySeconds = FName(TEXT("Shields.Regen.DelaySeconds"));

		inline const FName ShieldsRegenPeriodSeconds = FName(TEXT("Shields.Regen.PeriodSeconds"));

		inline const FName FighterMaxHealth = FName(TEXT("Fighter.MaxHealth"));

		inline const FName FighterMaxShields = FName(TEXT("Fighter.MaxShields"));
	}

	BREACHPOINT_API bool Evaluate(FName CurveName, float InputValue, float& OutValue);

	BREACHPOINT_API bool Evaluate(FName CurveName, float& OutValue);

	BREACHPOINT_API const UCurveTable* GetCombatCurveTable();

	BREACHPOINT_API void SetTableOverrideForTests(const UCurveTable* Table);
}
