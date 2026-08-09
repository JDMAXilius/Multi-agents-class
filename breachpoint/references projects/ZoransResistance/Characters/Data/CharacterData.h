// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ZoransResistance/AbilitySystem/Data/AbilitySystemData.h"
#include "CharacterData.generated.h"

class UZoransGameplayAbility;

USTRUCT(BlueprintType)
struct FCharacterData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Details")
	FText ClassDisplayName;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Details")
	TObjectPtr<UTexture2D> ThumbnailIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	FGrantedAbilitySystemData AbilitySystemData;
};

USTRUCT(BlueprintType)
struct FModularZoranComponentData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Modular Zoran Data")
	FPrimaryAssetId ZoranComponentDataAsset;
};

USTRUCT(BlueprintType)
struct FHologramData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Hologram Data")
	FPrimaryAssetId HologramDataAsset;
};