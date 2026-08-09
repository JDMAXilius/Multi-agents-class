// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "DA_WeaponSkin.generated.h"

class AZoransWeaponActor;

USTRUCT(BlueprintType)
struct FWeaponSkinDataAssetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Skin Data")
	FPrimaryAssetId WeaponSkinDataAsset;

	// This is used to determine if the WeaponSkin actually exists for the selected Weapon
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Skin Data")
	TMap<FName, TSoftObjectPtr<UTexture2D>> CompatibleWeapons;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Skin Data")
	bool bWeaponWrap = false;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Skin Data")
	bool bIsBundle = false;
};

USTRUCT(BlueprintType)
struct FWeaponSkinOverride
{
	GENERATED_BODY()

	// If this path is valid, we will spawn this weapon instead
	// The Material Instance set in CustomSkin will be applied to this weapon
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Skin Data")
	TSoftClassPtr<AZoransWeaponActor> WeaponOverrideClass;

	// This is the custom material that will be applied to the weapon if we are not using a Wrap
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Skin Data")
	TSoftObjectPtr<UMaterialInstance> WeaponSkin;
};

UCLASS()
class ZORANSRESISTANCE_API UDA_WeaponSkin : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Skin Data")
	FName InventoryDisplayName = NAME_None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Skin Data")
	FName StoreDisplayName = NAME_None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Weapon Skin Data")
	TSoftObjectPtr<UTexture2D> Thumbnail = nullptr;

	// These are custom skins designed specifically for the Weapon based on its FName/RowName
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Skin"), Category = "Zoran Attachment Data")
	TMap<FName, FWeaponSkinOverride> WeaponSkinOverrides;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Wrap"), Category = "Weapon Skin Data")
	TSoftObjectPtr<UTexture2D> Pattern;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Wrap"), Category = "Weapon Skin Data")
	TSoftObjectPtr<UTexture2D> Mask;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("DA_WeaponSkin", GetFName());
	}
};
