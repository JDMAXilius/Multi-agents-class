// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Engine/NetConnection.h"
#include "ZoransResistance/HMS/FHMSComponentSave.h"
#include "ZoransResistance/HMS/FHMSPropertySave.h"
#include "FHMSActorSave.generated.h"

USTRUCT(BlueprintType)
struct ZORANSRESISTANCE_API FHMSActorSave
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(Category = "ActorSave", SaveGame, BlueprintReadWrite)
	FTransform ActorTransformation;

	UPROPERTY(Category = "ActorSave", SaveGame, BlueprintReadWrite)
	int64 ActorID;

	UPROPERTY(Category = "ActorSave", SaveGame, BlueprintReadWrite)
	TArray<uint8> ActorData;

	UPROPERTY(Category = "ActorSave", SaveGame, BlueprintReadWrite)
	TMap<FString, FHMSComponentSave> ComponentData;

	UPROPERTY(Category = "ActorSave", SaveGame, BlueprintReadWrite)
	TMap<FString, FHMSPropertySave> CustomPropertyData;

public:
	FHMSActorSave()
	{
		ActorID = 0;
	};
	FHMSActorSave(AActor* Actor, TArray<FString> ComponentNames, TArray<FString> CustomPropertyNames);
	void Deserialize(AActor* Actor, TArray<FString> ComponentNames, TArray<FString> CustomPropertyNames);

	friend FArchive& operator<<(FArchive& Ar, FHMSActorSave& SG)
	{
		Ar << SG.ActorTransformation;
		Ar << SG.ActorData;
		Ar << SG.ComponentData;
		Ar << SG.CustomPropertyData;
		Ar << SG.ActorID;

		return Ar;
	}
};
