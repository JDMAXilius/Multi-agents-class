// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "DA_CelebrationTaunt.generated.h"

USTRUCT(BlueprintType)
struct FCelebrationTauntDataAssetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Celebration Taunt Data")
	FName DisplayName = NAME_None;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Celebration Taunt Data")
	FPrimaryAssetId WeaponSkinDataAsset;
};

UCLASS()
class ZORANSRESISTANCE_API UDA_CelebrationTaunt : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Celebration Taunt Data")
	TSoftObjectPtr<UTexture2D> Thumbnail = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Audio"), Category = "Celebration Taunt Data")
	TSoftObjectPtr<USoundWave> WinnerAudio = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Celebration Taunt Data")
	TSoftObjectPtr<UAnimMontage> Taunt = nullptr;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("DA_CelebrationTaunt", GetFName());
	}
};