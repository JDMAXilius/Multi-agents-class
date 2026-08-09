// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "FHMSPlayerSave.generated.h"

USTRUCT(BlueprintType)
struct ZORANSRESISTANCE_API FHMSPlayerSave
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(Category = "PlayerSave", SaveGame, BlueprintReadWrite)
	FString PlayerID;

	UPROPERTY(Category = "PlayerSave", SaveGame, BlueprintReadWrite)
	TArray<uint8> PlayerStateData;

	UPROPERTY(Category = "PlayerSave", SaveGame, BlueprintReadWrite)
	TArray<uint8> PlayerControllerData;


public:
	FHMSPlayerSave()
	{

	};

	FHMSPlayerSave(FString InPlayerID,
		TArray<uint8> InPlayerStateData,
		TArray<uint8> InPlayerControllerData
	)
	{
		PlayerID = InPlayerID;
		PlayerStateData = InPlayerStateData;
		PlayerControllerData = InPlayerControllerData;
	}

	friend FArchive& operator<<(FArchive& Ar, FHMSPlayerSave& SG)
	{
		Ar << SG.PlayerStateData;
		Ar << SG.PlayerID;
		Ar << SG.PlayerControllerData;

		return Ar;
	}
};
