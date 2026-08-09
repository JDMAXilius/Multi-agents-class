// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "FileMediaSource.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "Sound/SoundCue.h"
#include "DA_Throwdown_CR.generated.h"

USTRUCT(BlueprintType)
struct FThrowdownCRData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Throwdown Close Range Data")
	FPrimaryAssetId ThrowdownCRDataAsset;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Throwdown Close Range Data")
	FName ParodyRowName = NAME_None;
};

UCLASS()
class ZORANSRESISTANCE_API UDA_Throwdown_CR : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwdown Close Range Data")
	FName DisplayName = NAME_None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Throwdown Close Range Data")
	TSoftObjectPtr<UTexture2D> Thumbnail = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Throwdown Close Range Data")
	TSoftObjectPtr<UAnimMontage> AttackerMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Throwdown Close Range Data")
	TSoftObjectPtr<UAnimMontage> VictimMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Throwdown Close Range Data")
	TSoftObjectPtr<USoundCue> ThrowdownAudio = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwdown Close Range Data")
	UEnvQuery* EQS = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwdown Close Range Data")
	TMap<FName, float> EQS_QueryParams = { {FName("Distance_Min"), 128.f}, {FName("Distance_Max"), 180.f} };
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Media"), Category = "Throwdown Close Range Data")
	TSoftObjectPtr<UFileMediaSource> PreviewMedia = nullptr;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("DA_Throwdown_CR", GetFName());
	}
};
