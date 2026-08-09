#pragma once
// Base data assets: UOSDataAsset, UOSAbilityDataAsset (costs, montage), UOSAttackDataAsset (hits, snap). Used as asset base classes.

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/OSAbilityCostAndEffects.h"
#include "Data/OSAttackComboData.h"
#include "OSPrimaryDataAssets.generated.h"

UCLASS()
class ONSIGHT_API UOSDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
};

UCLASS()
class ONSIGHT_API UOSAbilityDataAsset : public UOSDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
	TArray<FOSResource> Costs;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> Montage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName SectionName;
};

UCLASS()
class ONSIGHT_API UOSAttackDataAsset : public UOSAbilityDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TArray<FOSAttackHitInfo> Hits;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float AutoLockRange = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float SnapRange = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float SnapDuration = .2f;
};
