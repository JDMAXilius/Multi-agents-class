#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BNAssetSettings.generated.h"

class UDataTable;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Breachpoint Next"))
class BREACHPOINTNEXT_API UBNAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UBNAssetSettings* Get();

	virtual FName GetCategoryName() const override;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> WeaponTable;
};
