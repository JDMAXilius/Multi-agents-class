// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Engine/NetConnection.h"
#include "ZoransResistance/HMS/FHMSActorSave.h"
#include "Serialization/StructuredArchive.h"
#include "FHMSActorState.generated.h"

USTRUCT(BlueprintType)
struct ZORANSRESISTANCE_API FHMSActorState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(Category = "ActorState", SaveGame, BlueprintReadWrite)
	FString ControllerID;

	UPROPERTY(Category = "ActorState", SaveGame, BlueprintReadWrite)
	FString ActorClass;

	UPROPERTY(Category = "ActorState", SaveGame, BlueprintReadWrite)
	FHMSActorSave ActorSave;

public:
	FHMSActorState()
	{

	};

	friend FArchive& operator<<(FArchive& Ar, FHMSActorState& SG)
	{
		Ar << SG.ControllerID;
		Ar << SG.ActorClass;
		Ar << SG.ActorSave;

		return Ar;
	}
};
