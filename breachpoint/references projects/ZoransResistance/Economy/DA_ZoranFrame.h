// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "DA_ZoranFrame.generated.h"


USTRUCT(BlueprintType)
struct FZoranFrameDataAssetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Zoran Frame Data")
	FPrimaryAssetId ZoranFrameDataAsset;
};

UCLASS()
class ZORANSRESISTANCE_API UDA_ZoranFrame : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Frame Data")
	FName DisplayName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Zoran Frame Data")
	TSoftObjectPtr<UMaterialInstance> FrameMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Zoran Frame Data")
	float Radius = 1.f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("DA_ZoranFrame", GetFName());
	}
};
