// Breachpoint. The ONE reader of CT_Combat — every combat coefficient enters the sim here.
#include "AbilitySystem/BRCombatCurves.h"

#include "Engine/CurveTable.h"

#include "Core/BRCore.h"

UBRCombatCurveConfig::UBRCombatCurveConfig()
{
	CombatCurveTable = TSoftObjectPtr<UCurveTable>(FSoftObjectPath(TEXT("/Game/Data/CT_Combat.CT_Combat")));
}

namespace BRCombatCurves
{
	namespace Private
	{
		static TWeakObjectPtr<const UCurveTable> CachedTable;

		static TWeakObjectPtr<const UCurveTable> TestOverrideTable;

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

		Private::ReportedMissingCurves().Reset();
	}

	bool Evaluate(FName CurveName, float InputValue, float& OutValue)
	{
		const UCurveTable* Table = GetCombatCurveTable();
		if (!Table)
		{
			return false;
		}

		const FRealCurve* Curve = Table->FindCurve(CurveName, FString(), false);
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
