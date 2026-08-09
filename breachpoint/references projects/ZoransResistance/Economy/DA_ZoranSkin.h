// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "DA_ZoranSkin.generated.h"


USTRUCT(BlueprintType)
struct FZoranSkinDataAssetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Zoran Skin Data")
	FPrimaryAssetId ZoranSkinDataAsset;
};

UCLASS()
class ZORANSRESISTANCE_API UDA_ZoranSkin : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Skin Data")
	FName DisplayName = NAME_None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Zoran Skin Data")
	TSoftObjectPtr<UTexture2D> Thumbnail = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Skin Data")
	TSoftObjectPtr<UTexture2D> Texture_BaseColor = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Skin Data")
	TSoftObjectPtr<UTexture2D> Texture_Normal_EmissiveAlpha = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Skin Data")
	TSoftObjectPtr<UTexture2D> Texture_ORM = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Skin Data")
	TSoftObjectPtr<UTexture2D> Texture_SwatchMask = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Skin Data")
	bool bHasSwatch = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Skin Data")
	FName SwatchRowName = NAME_None;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("DA_ZoranSkin", GetFName());
	}
};
