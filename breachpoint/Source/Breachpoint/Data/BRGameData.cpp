#include "Data/BRGameData.h"

#include "Core/BRCore.h"
#include "Data/BRDataRows.h"
#include "Engine/AssetManager.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"

void UBRGameData::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TArray<FSoftObjectPath> Paths;
	if (WeaponTablePath.IsValid())
	{
		Paths.Add(WeaponTablePath);
	}
	else
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGameData: WeaponTablePath is unset in DefaultGame.ini — every weapon lookup will answer null."));
	}

	if (CombatCurveTablePath.IsValid())
	{
		Paths.Add(CombatCurveTablePath);
	}
	else
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGameData: CombatCurveTablePath is unset in DefaultGame.ini — every curve eval will answer 0."));
	}

	if (Paths.IsEmpty())
	{
		return;
	}

	// The module's ONLY RequestAsyncLoad call site (BP91). If the assets are already
	// resident the delegate fires inline, so lookups are valid from first frame.
	TableLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths, FStreamableDelegate::CreateUObject(this, &UBRGameData::OnTablesLoaded));
}

void UBRGameData::Deinitialize()
{
	if (TableLoadHandle.IsValid())
	{
		TableLoadHandle->ReleaseHandle();
		TableLoadHandle.Reset();
	}
	WeaponTable = nullptr;
	CombatCurveTable = nullptr;
	Super::Deinitialize();
}

void UBRGameData::OnTablesLoaded()
{
	if (WeaponTablePath.IsValid())
	{
		WeaponTable = Cast<UDataTable>(WeaponTablePath.ResolveObject());
		if (!WeaponTable)
		{
			UE_LOG(LogBRCombat, Warning,
				TEXT("BRGameData: '%s' did not resolve to a UDataTable — path typo or wrong asset type."),
				*WeaponTablePath.ToString());
		}
		else if (WeaponTable->GetRowStruct() != FBRWeaponRow::StaticStruct())
		{
			UE_LOG(LogBRCombat, Warning,
				TEXT("BRGameData: '%s' is not built on FBRWeaponRow — GetWeaponRow will answer null."),
				*WeaponTablePath.ToString());
			WeaponTable = nullptr;
		}
	}

	if (CombatCurveTablePath.IsValid())
	{
		CombatCurveTable = Cast<UCurveTable>(CombatCurveTablePath.ResolveObject());
		if (!CombatCurveTable)
		{
			UE_LOG(LogBRCombat, Warning,
				TEXT("BRGameData: '%s' did not resolve to a UCurveTable — path typo or wrong asset type."),
				*CombatCurveTablePath.ToString());
		}
	}
}

const FBRWeaponRow* UBRGameData::GetWeaponRow(FName RowName) const
{
	if (!WeaponTable)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGameData::GetWeaponRow('%s'): weapon table not loaded — null answer."),
			*RowName.ToString());
		return nullptr;
	}

	const FBRWeaponRow* Row =
		WeaponTable->FindRow<FBRWeaponRow>(RowName, TEXT("BRGameData::GetWeaponRow"), /*bWarnIfRowMissing*/ false);
	if (!Row)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGameData::GetWeaponRow('%s'): no such row in '%s' — null answer, nothing defaults."),
			*RowName.ToString(), *WeaponTable->GetName());
	}
	return Row;
}

float UBRGameData::EvalCombatCurve(FName CurveName, float InTime) const
{
	if (!CombatCurveTable)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGameData::EvalCombatCurve('%s'): curve table not loaded — answering 0."),
			*CurveName.ToString());
		return 0.f;
	}

	const FRealCurve* Curve =
		CombatCurveTable->FindCurve(CurveName, TEXT("BRGameData::EvalCombatCurve"), /*bWarnIfNotFound*/ false);
	if (!Curve)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGameData::EvalCombatCurve('%s'): no such curve in '%s' — answering 0, not a coefficient."),
			*CurveName.ToString(), *CombatCurveTable->GetName());
		return 0.f;
	}

	return Curve->Eval(InTime);
}
