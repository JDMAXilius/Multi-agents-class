// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Engine/NetConnection.h"
#include "Serialization/StructuredArchive.h"
#include "FHMSActorReference.generated.h"

USTRUCT(BlueprintType)
struct ZORANSRESISTANCE_API FHMSActorReference
{

	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	AActor* ActorRef;

	UPROPERTY(Category = "ActorReference", SaveGame, BlueprintReadOnly)
	int64 ActorID;

	UPROPERTY(Category = "ActorReference", SaveGame, BlueprintReadOnly)
	TSubclassOf<AActor> ActorClass;



	FHMSActorReference()
	{
		ActorRef = nullptr;
		ActorID = 0;
	};

	FHMSActorReference(int64 ID, TSubclassOf<AActor> Class)
	{
		ActorID = ID;
		ActorClass = Class;
		ActorRef = nullptr;
	};

	AActor* GetReference(UObject* WorldContextObject);

	friend FArchive& operator<<(FArchive& Ar, FHMSActorReference& SG)
	{
		Ar << SG.ActorID;
		Ar << SG.ActorClass;

		return Ar;
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << *this;

		bOutSuccess = true;
		return true;
	}

};

template<>
struct TStructOpsTypeTraits<FHMSActorReference> : public TStructOpsTypeTraitsBase2<FHMSActorReference>
{
	enum
	{
		WithNetSerializer = true
	};
};
