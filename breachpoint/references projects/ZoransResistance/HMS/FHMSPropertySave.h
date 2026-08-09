// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "FHMSPropertySave.generated.h"

USTRUCT(BlueprintType)
struct ZORANSRESISTANCE_API FHMSPropertySave
{

	GENERATED_USTRUCT_BODY()

	UPROPERTY(Category = "PropertySave", SaveGame, BlueprintReadWrite)
	TArray<uint8> PropertyData;

public:
	FHMSPropertySave(const TArray<uint8>& CD) : PropertyData(CD)
	{

	};

	FHMSPropertySave()
	{

	};

	friend FArchive& operator<<(FArchive& Ar, FHMSPropertySave& SG)
	{
		Ar << SG.PropertyData;

		return Ar;
	}

};
