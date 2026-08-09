// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Engine/NetConnection.h"
#include "ZoransResistance/HMS/FHMSPlayerSave.h"
#include "ZoransResistance/HMS/FHMSActorState.h"
#include "Serialization/StructuredArchive.h"
#include "FHMSGameSave.generated.h"

USTRUCT(BlueprintType)
struct ZORANSRESISTANCE_API FHMSGameSave
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY(Category = "GameSave", SaveGame, BlueprintReadWrite)
	TArray<FHMSPlayerSave> Players;
	UPROPERTY(Category = "GameSave", SaveGame, BlueprintReadWrite)
	TArray<FHMSActorState> Actors;
	UPROPERTY(Category = "GameSave", SaveGame, BlueprintReadWrite)
	TArray<uint8> GameModeSave;
	UPROPERTY(Category = "GameSave", SaveGame, BlueprintReadWrite)
	TArray<uint8> GameStateSave;
	UPROPERTY(Category = "GameSave", SaveGame, BlueprintReadWrite)
	FString LevelName;
public:

	friend FArchive& operator<<(FArchive& Ar, FHMSGameSave& SG)
	{
		Ar << SG.Players;
		Ar << SG.GameModeSave;
		Ar << SG.GameStateSave;
		Ar << SG.Actors;
		Ar << SG.LevelName;

		return Ar;
	}

	FHMSGameSave()
	{

	};

	FHMSGameSave(TArray<FHMSPlayerSave> InPlayers,
		TArray<FHMSActorState> InActors,
		TArray<uint8> InGameModeSave,
		TArray<uint8> InGameStateSave,
		FString InLevelName)
	{
		Players = InPlayers;
		Actors = InActors;
		GameModeSave = InGameModeSave;
		GameStateSave = InGameStateSave;
		LevelName = InLevelName;
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << *this;

		bOutSuccess = true;
		return true;
	}

};

template<>
struct TStructOpsTypeTraits<FHMSGameSave> : public TStructOpsTypeTraitsBase2<FHMSGameSave>
{
	enum
	{
		WithNetSerializer = true
	};
};

