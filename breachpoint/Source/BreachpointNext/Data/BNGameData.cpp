#include "Data/BNGameData.h"

#include "Data/BNAssetSettings.h"
#include "Data/BNDataRows.h"
#include "Engine/DataTable.h"

void UBNGameData::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const UBNAssetSettings* Settings = UBNAssetSettings::Get())
	{
		WeaponTable = Settings->WeaponTable.LoadSynchronous();
	}
}

const FBNWeaponRow* UBNGameData::FindWeaponRow(FName RowName) const
{
	if (!WeaponTable || RowName.IsNone())
	{
		return nullptr;
	}
	return WeaponTable->FindRow<FBNWeaponRow>(RowName, FString(), /*bWarnIfMissing=*/false);
}
