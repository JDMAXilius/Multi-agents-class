// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ZoransCosmeticData.generated.h"


USTRUCT(BlueprintType)
struct FZoranColorSwatch : public FTableRowBase
{
	GENERATED_BODY()
 
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Zoran Cosmetic Data")
	FLinearColor Color_Primary = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Zoran Cosmetic Data")
	FLinearColor Color_Secondary = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Zoran Cosmetic Data")
	FLinearColor Color_Tertiary = FLinearColor::White;
};
