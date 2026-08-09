// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "FHMSComponentSave.generated.h"

USTRUCT(BlueprintType)
struct ZORANSRESISTANCE_API FHMSComponentSave
{

	GENERATED_USTRUCT_BODY()

	UPROPERTY(Category = "ComponentSave", SaveGame, BlueprintReadWrite)
	TArray<uint8> ComponentData;

public:
	FHMSComponentSave(const TArray<uint8>& CD) : ComponentData(CD)
	{

	};

	FHMSComponentSave()
	{

	};

	friend FArchive& operator<<(FArchive& Ar, FHMSComponentSave& SG)
	{
		Ar << SG.ComponentData;

		return Ar;
	}

};
