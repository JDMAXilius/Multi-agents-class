// Breachpoint. The ONE reader of CT_Combat — every combat coefficient enters the sim here.

#include "AbilitySystem/BRCombatCurves.h"

#include "Engine/CurveTable.h"

#include "Core/BRCore.h"

UBRCombatCurveConfig::UBRCombatCurveConfig()
{
	// A soft PATH default, not an asset reference: nothing is loaded by writing this line, and
	// DefaultGame.ini can point the whole sim at a different table without a recompile. The
	// package path is Content/Data/CT_Combat.csv's imported destination (data-and-assets.md).
	CombatCurveTable = TSoftObjectPtr<UCurveTable>(FSoftObjectPath(TEXT("/Game/Data/CT_Combat.CT_Combat")));
}

namespace BRCombatCurves
{
	namespace Private
	{
		/**
		 * Cached resolution of the configured table. Weak, so a table unloaded between PIE runs is
		 * re-resolved rather than dangling.
		 */
		static TWeakObjectPtr<const UCurveTable> CachedTable;

		/** Set by SetTableOverrideForTests. Wins over the configured table while it is set. */
		static TWeakObjectPtr<const UCurveTable> TestOverrideTable;

		/** One Error per missing curve name per process. A per-shot Error log is a log nobody reads. */
		static TSet<FName>& ReportedMissingCurves()
		{
			static TSet<FName> Reported;
			return Reported;
		}
	}

	const UCurveTable* GetCombatCurveTable()
	{
		if (const UCurveTable* Override = Private::TestOverrideTable.Get())
		{
			return Override;
		}

		if (const UCurveTable* Cached = Private::CachedTable.Get())
		{
			return Cached;
		}

		const UBRCombatCurveConfig* Config = GetDefault<UBRCombatCurveConfig>();
		if (!Config || Config->CombatCurveTable.IsNull())
		{
			UE_LOG(LogBRCombat, Error, TEXT("BRCombatCurves: no CT_Combat configured (UBRCombatCurveConfig::CombatCurveTable is null). Every combat coefficient is unavailable."));
			return nullptr;
		}

		// Synchronous, and only here. This is a LOAD POINT in the data contract's sense: the first
		// coefficient read of the process. If it ever shows in a profile, the fix is to preload the
		// table at match start, NOT to cache a copy of the numbers somewhere else.
		const UCurveTable* Table = Config->CombatCurveTable.LoadSynchronous();
		if (!Table)
		{
			UE_LOG(LogBRCombat, Error, TEXT("BRCombatCurves: CT_Combat failed to load from '%s'. Every combat coefficient is unavailable."),
				*Config->CombatCurveTable.ToString());
			return nullptr;
		}

		Private::CachedTable = Table;
		return Table;
	}

	void SetTableOverrideForTests(const UCurveTable* Table)
	{
		Private::TestOverrideTable = Table;

		// A spec that swaps the table must not inherit the previous table's "already reported"
		// state, or its own missing-curve assertions go quiet.
		Private::ReportedMissingCurves().Reset();
	}

	bool Evaluate(FName CurveName, float InputValue, float& OutValue)
	{
		const UCurveTable* Table = GetCombatCurveTable();
		if (!Table)
		{
			return false;
		}

		// FindCurve with a null context string and bWarnIfNotFound=false: the warning below is ours,
		// deduplicated and on our channel, and it names the consequence rather than the lookup.
		const FRealCurve* Curve = Table->FindCurve(CurveName, /*ContextString=*/FString(), /*bWarnIfNotFound=*/false);
		if (!Curve)
		{
			bool bAlreadyReported = false;
			Private::ReportedMissingCurves().Add(CurveName, &bAlreadyReported);
			if (!bAlreadyReported)
			{
				UE_LOG(LogBRCombat, Warning, TEXT("BRCombatCurves: CT_Combat has no curve '%s'. The caller decides what that means; nothing was substituted here."),
					*CurveName.ToString());
			}
			return false;
		}

		OutValue = Curve->Eval(InputValue);
		return true;
	}

	bool Evaluate(FName CurveName, float& OutValue)
	{
		return Evaluate(CurveName, 0.f, OutValue);
	}
}
