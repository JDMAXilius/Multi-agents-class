// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DA_Hologram.generated.h"

class USoundCue;
class UIKRetargeter;

UCLASS()
class ZORANSRESISTANCE_API UDA_Hologram : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hologram Data")
	FName DisplayName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Hologram Data")
	TSoftObjectPtr<UTexture2D> Thumbnail = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Hologram Data")
	TSoftObjectPtr<USkeletalMesh> Hologram = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Hologram Data")
	TSoftObjectPtr<UMaterialInstance> Material = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Hologram Data")
	TSoftObjectPtr<UIKRetargeter> IKRetargeter = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Hologram Data")
	TSoftClassPtr<AActor> AdditionalActorClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Hologram Data")
	TSoftObjectPtr<USoundCue> ActivationSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Hologram Data")
	TSoftObjectPtr<USoundWave> ThemeMusic = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hologram Data")
	bool bSelectable = true;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("DA_Hologram", GetFName());
	}
};
