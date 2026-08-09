// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayAbility.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayEffect.h"
#include "DA_ZoranComponent.generated.h"


UCLASS()
class ZORANSRESISTANCE_API UDA_ZoranComponent : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Modular Zoran Data")
	FName DisplayName = NAME_None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "UI"), Category = "Modular Zoran Data")
	TSoftObjectPtr<UTexture2D> Thumbnail = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Modular Zoran Data")
	TSoftObjectPtr<USkeletalMesh> ZoranComponentMesh = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Modular Zoran Data")
	TArray<TSoftClassPtr<UZoransGameplayAbility>> ZoranComponentAbilities;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AssetBundles = "Game"), Category = "Modular Zoran Data")
	TArray<TSubclassOf<UZoransGameplayEffect>> ZoranComponentEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Modular Zoran Data")
	EModularZoranComponent ZoranComponentSlot = EModularZoranComponent::None;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("DA_ZoranComponent", GetFName());
	}
};
