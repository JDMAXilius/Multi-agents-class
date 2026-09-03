#include "Data/BNAssetSettings.h"

const UBNAssetSettings* UBNAssetSettings::Get()
{
	return GetDefault<UBNAssetSettings>();
}

FName UBNAssetSettings::GetCategoryName() const
{
	return FName(TEXT("Breachpoint Next"));
}
