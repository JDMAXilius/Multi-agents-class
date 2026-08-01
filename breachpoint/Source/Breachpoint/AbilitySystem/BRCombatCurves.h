// Breachpoint. The ONE reader of CT_Combat — every combat coefficient enters the sim here.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "BRCombatCurves.generated.h"

class UCurveTable;

/**
 * UBRCombatCurveConfig — where the CT_Combat asset reference lives, and nothing else.
 *
 * A `config` UObject rather than a UDeveloperSettings on purpose: DeveloperSettings would add a
 * module dependency to Breachpoint.Build.cs, a file four packets are editing this week. This
 * class needs only CoreUObject, and GetDefault<>() reads it with no instance anywhere.
 *
 * The reference is SOFT (data-and-assets.md law: "every asset reference ... is TSoftObjectPtr").
 * The default path below is a PATH, not an asset: no ConstructorHelpers, no hard pointer, no
 * cook dependency from code. Override it in DefaultGame.ini to point a test or a variant at a
 * different table.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "BR Combat Curve Config"))
class BREACHPOINT_API UBRCombatCurveConfig : public UObject
{
	GENERATED_BODY()

public:
	UBRCombatCurveConfig();

	/** CT_Combat — the CurveTable holding every damage/movement/regen coefficient. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Breachpoint|Combat")
	TSoftObjectPtr<UCurveTable> CombatCurveTable;
};

/**
 * BRCombatCurves — the sim's read side of CT_Combat.
 *
 * WHY THIS FILE EXISTS AT ALL. Every gameplay number in this project lives in data (law 3), and
 * CT_Combat is a CurveTable — named curves, no row struct, addressed by curve name + input
 * (data-and-assets.md). A CurveTable cannot be referenced from a UGameplayEffect constructor the
 * way an editor-authored FScalableFloat would reference it, because that reference is HARD and
 * hard asset refs in C++ are banned. So the coefficient does not travel with the effect: it is
 * read HERE, on the server, and handed to the effect as a SetByCaller magnitude or read directly
 * by the CMC. One reader, one spelling of each curve name, one failure mode.
 *
 * DETERMINISM. Evaluate() is a pure function of (table contents, curve name, input). There is no
 * time, no frame delta, no randomness. Given the same CSV, two machines return the same float —
 * which is what lets a pinned spec assert an exact number and the server reproduce a disputed
 * result.
 *
 * FAILURE IS LOUD AND TYPED, never a silent default. Evaluate() returns false and writes nothing
 * when the table or the curve is missing; every caller decides what a missing coefficient means
 * for IT and says so at the call site. There is deliberately no "EvaluateOrDefault(1.0f)" here —
 * that function is how a broken table becomes a balance change nobody can see.
 */
namespace BRCombatCurves
{
	/**
	 * The curve names this packet reads. Constants, not literals scattered at call sites: a
	 * misspelling here fails once and visibly instead of silently returning "curve not found"
	 * from one of two callers.
	 *
	 * These are DATA KEYS, not gameplay numbers — the values live in Content/Data/CT_Combat.csv.
	 */
	namespace Names
	{
		/** Suffix appended to a Damage.* tag name to form its multiplier curve. */
		inline const TCHAR* DamageMultiplierSuffix = TEXT(".Multiplier");

		/** Walk-speed multiplier applied by the CMC while the sprint saved-move flag is set. */
		inline const FName MovementSprintSpeedMultiplier = FName(TEXT("Movement.Sprint.SpeedMultiplier"));

		/** Shield regen, points per SECOND (GE_Regen converts to per-period). */
		inline const FName ShieldsRegenRatePerSecond = FName(TEXT("Shields.Regen.RatePerSecond"));

		/** Seconds of State.Combat.RecentDamage after a hit — GE_RecentDamage's duration. */
		inline const FName ShieldsRegenDelaySeconds = FName(TEXT("Shields.Regen.DelaySeconds"));

		/** GE_Regen's tick period. Granularity of the regen, not its rate. */
		inline const FName ShieldsRegenPeriodSeconds = FName(TEXT("Shields.Regen.PeriodSeconds"));

		/** GE_InitStats: the fighter's health capacity. */
		inline const FName FighterMaxHealth = FName(TEXT("Fighter.MaxHealth"));

		/** GE_InitStats: the fighter's shield capacity. */
		inline const FName FighterMaxShields = FName(TEXT("Fighter.MaxShields"));
	}

	/**
	 * Evaluate CurveName at InputValue.
	 *
	 * @return true and writes OutValue when the table AND the curve exist; false and writes
	 *         nothing otherwise. There is no third outcome and no default.
	 */
	BREACHPOINT_API bool Evaluate(FName CurveName, float InputValue, float& OutValue);

	/** Evaluate at input 0 — the shape every constant coefficient in CT_Combat uses. */
	BREACHPOINT_API bool Evaluate(FName CurveName, float& OutValue);

	/**
	 * The resolved CT_Combat asset, or null. Synchronously loads on first use (a load point:
	 * the first damage event or the first sprint of the process), then caches.
	 */
	BREACHPOINT_API const UCurveTable* GetCombatCurveTable();

	/**
	 * Point the sim at a CurveTable built in memory, for headless specs.
	 *
	 * THE TEST SEAM, and it is deliberate: a pinned suite must be able to state the exact
	 * coefficients it is pinning without shipping an asset or reaching a content path. Pass
	 * nullptr to restore the configured table. Never called from gameplay code — a grep for
	 * this symbol outside Tests/ is a finding.
	 */
	BREACHPOINT_API void SetTableOverrideForTests(const UCurveTable* Table);
}
