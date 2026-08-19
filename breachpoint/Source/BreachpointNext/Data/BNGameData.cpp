#include "Data/BNGameData.h"

#include "AI/BNBotBrain.h"
#include "BreachpointNext.h"
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

	// Same load shape as the weapon table; the soft path is ini-set on THIS class (R6 G3 3.2).
	BotAmbitionTable = BotAmbitionsTable.LoadSynchronous();
}

const FBNWeaponRow* UBNGameData::FindWeaponRow(FName RowName) const
{
	if (!WeaponTable || RowName.IsNone())
	{
		return nullptr;
	}
	return WeaponTable->FindRow<FBNWeaponRow>(RowName, FString(), /*bWarnIfMissing=*/false);
}

const FBNBotAmbitionRow* UBNGameData::FindBotAmbitionRow(EBNBotAmbition Ambition) const
{
	if (!BotAmbitionTable)
	{
		if (!bWarnedNoAmbitionTable)
		{
			bWarnedNoAmbitionTable = true;
			UE_LOG(LogBN, Warning, TEXT("BNGameData: BotAmbitionsTable is unset or failed to load — bot brains run on the C++ default ambition rows."));
		}
		return nullptr;
	}
	return BotAmbitionTable->FindRow<FBNBotAmbitionRow>(UBNBotBrain::AmbitionRowName(Ambition), FString(), /*bWarnIfMissing=*/false);
}
