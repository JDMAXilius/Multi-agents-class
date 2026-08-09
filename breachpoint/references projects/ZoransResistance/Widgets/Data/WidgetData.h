// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WidgetData.generated.h"

USTRUCT(BlueprintType)
struct FGameModeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TSubclassOf<AGameModeBase> GameModeClass = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float GameModeTimeLimit = 600.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 ScoreLimit = 50;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	bool bUsesAI = true;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TArray<UDataLayerAsset*> GameModeRelatedDataLayers;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FName LevelMusic = "Default";
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FName VictoryMusic = "Default";

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FName DefeatMusic = "Default";
};

USTRUCT(BlueprintType)
struct FExperienceData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Details")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Details")
	FText ShortDescription;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Details")
	FText LongDescription;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Details")
	UTexture2D* Thumbnail = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Details")
	TSoftObjectPtr<UWorld> Level = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Details")
	TMap<FString, FGameModeInfo> AvailableGameModes;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Details")
	TMap<FString, UDataLayerAsset*> DataLayerMappings;

	// If false, will not be picked / loaded in any experience selections
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Development")
	bool bUsedInGame = true;
	
	// If true, will not display in public builds unless host is a dev
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Development")
	bool bInternalUseOnly = false;
};

USTRUCT(BlueprintType)
struct FBattlePassItemInfo
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Level = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bElite = false;
};

UENUM(BlueprintType)
enum class EPartyInviteStatus : uint8
{
	NotInvited,
	Invited,
	InParty
};
