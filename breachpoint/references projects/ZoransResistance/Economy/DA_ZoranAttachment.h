// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "ZoransResistance/Game/Data/StaticGameData.h"
#include "DA_ZoranAttachment.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FZoranAttachmentStaticDataAssetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Zoran Attachment Data")
	FPrimaryAssetId ZoranAttachmentStaticDataAsset;
};

USTRUCT(BlueprintType)
struct FZoranAttachmentSkeletalDataAssetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Zoran Attachment Data")
	FPrimaryAssetId ZoranAttachmentSkeletalDataAsset;
};

UCLASS()
class ZORANSRESISTANCE_API UDA_ZoranAttachment_Static : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	FName DisplayName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Zoran Attachment Data")
	TSoftObjectPtr<UTexture2D> Thumbnail = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Attachment Data")
	TSoftObjectPtr<UStaticMesh> PrimaryMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Attachment Data")
	TSoftObjectPtr<UMaterialInstance> PrimaryMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	FName PrimaryAttachmentSocket = NAME_None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Attachment Data")
	TSoftObjectPtr<UStaticMesh> SecondaryMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Attachment Data")
	TSoftObjectPtr<UMaterialInstance> SecondaryMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	FName SecondaryAttachmentSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	bool bMirrorPrimaryAsSecondary = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	EZoranBodySection BodySection = EZoranBodySection::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	EZoranRarity Rarity = EZoranRarity::None;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("DA_ZoranAttachment_Static", GetFName());
	}
};

UCLASS()

class ZORANSRESISTANCE_API UDA_ZoranAttachment_Skeletal : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	FName DisplayName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Zoran Attachment Data")
	TSoftObjectPtr<UTexture2D> Thumbnail = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Attachment Data")
	TSoftObjectPtr<USkeletalMesh> PrimaryMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Attachment Data")
	TSoftObjectPtr<UMaterialInstance> PrimaryMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	FName PrimaryAttachmentSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Attachment Data")
	TSoftObjectPtr<USkeletalMesh> SecondaryMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Zoran Attachment Data")
	TSoftObjectPtr<UMaterialInstance> SecondaryMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	FName SecondaryAttachmentSocket = NAME_None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Zoran Attachment Data")
	bool bMirrorPrimaryAsSecondary = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	EZoranBodySection BodySection = EZoranBodySection::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoran Attachment Data")
	EZoranRarity Rarity = EZoranRarity::None;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("DA_ZoranAttachment_Skeletal", GetFName());
	}
};