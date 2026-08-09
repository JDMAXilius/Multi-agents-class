// Copyright 2022 Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "ZoransResistance/AbilitySystem/Data/AbilitySystemData.h"
#include "ZoransResistance/Projectiles/ZoransProjectileBase.h"
#include "ZoransResistance/Projectiles/ZoransProjectileBouncing.h"
#include "ZoransTargetActor.generated.h"

UENUM(BlueprintType)
enum class EZoransTargetDataType : uint8
{
	None							UMETA(DisplayName = "None"),
	Hitscan							UMETA(DisplayName = "Hitscan"),
	Projectile_Ballistic			UMETA(DisplayName = "Ballistic Projectile"),
	Projectile_Bouncing				UMETA(DisplayName = "Bouncing Projectile")
};

USTRUCT(BlueprintType)
struct FHitscanTargetData : public FGameplayAbilityTargetData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Target Data")
	AActor* HitActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Target Data")
	AActor* SourceActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Target Data")
	UPrimitiveComponent* HitComponent = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Target Data")
	FVector RelativeHitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Target Data")
	FVector SurfaceNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Target Data")
	ETargetHitType TargetHitType = ETargetHitType::None;

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Target Data")
	FName HitBone = NAME_None;
			
	virtual bool HasOrigin() const override
	{
		return false;
	}

	virtual bool HasEndPoint() const override
	{
		return false;
	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FHitscanTargetData::StaticStruct();
	}

	virtual FString ToString() const override
	{
		return TEXT("FGameplayAbilityHitscanData");
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

inline bool FHitscanTargetData::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	RelativeHitLocation.NetSerialize(Ar, Map, bOutSuccess);
	SurfaceNormal.NetSerialize(Ar, Map, bOutSuccess);

	Ar << HitActor;
	Ar << SourceActor;
	Ar << HitComponent;
	Ar << HitBone;
	Ar << TargetHitType;

	bOutSuccess = true;
	return true;
}

template<>
struct TStructOpsTypeTraits<FHitscanTargetData> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetDataHandle>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

USTRUCT(BlueprintType)
struct FBaseProjectileSpawnTargetData : public FGameplayAbilityTargetData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	FVector SpawnLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	FVector SpawnDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	float LocalSpawnTime = -1.f;

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	int32 ProjectileIndex = -1;

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	FName SpawnSocketOverride = NAME_None;
		
	virtual bool HasOrigin() const override
	{
		return false;
	}

	virtual bool HasEndPoint() const override
	{
		return false;
	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FBaseProjectileSpawnTargetData::StaticStruct();
	}

	virtual FString ToString() const override
	{
		return TEXT("FGameplayAbilityBaseProjectileSpawnData");
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

inline bool FBaseProjectileSpawnTargetData::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	SpawnLocation.NetSerialize(Ar, Map, bOutSuccess);
	SpawnDirection.NetSerialize(Ar, Map, bOutSuccess);
	
	Ar << LocalSpawnTime;
	Ar << ProjectileIndex;
	Ar << SpawnSocketOverride;

	bOutSuccess = true;
	return true;
}

template<>
struct TStructOpsTypeTraits<FBaseProjectileSpawnTargetData> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetDataHandle>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

USTRUCT(BlueprintType)
struct FBouncingProjectileSpawnTargetData : public FGameplayAbilityTargetData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	FVector SpawnLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	FVector SpawnDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	float LocalSpawnTime = -1.f;

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	int32 ProjectileIndex = -1;

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	FName SpawnSocketOverride = NAME_None;
	
	// BOUNCING

	UPROPERTY(BlueprintReadWrite, Category = "Projectile Spawn Data")
	TArray<FBouncingProjectilePathSegment> ProjectileBouncePath;
		
	virtual bool HasOrigin() const override
	{
		return false;
	}

	virtual bool HasEndPoint() const override
	{
		return false;
	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FBouncingProjectileSpawnTargetData::StaticStruct();
	}

	virtual FString ToString() const override
	{
		return TEXT("FGameplayAbilityBouncingProjectileSpawnData");
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

inline bool FBouncingProjectileSpawnTargetData::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	SpawnLocation.NetSerialize(Ar, Map, bOutSuccess);
	SpawnDirection.NetSerialize(Ar, Map, bOutSuccess);
	SafeNetSerializeTArray_WithNetSerialize<8>(Ar, ProjectileBouncePath, Map);
	
	Ar << LocalSpawnTime;
	Ar << ProjectileIndex;
	Ar << SpawnSocketOverride;

	bOutSuccess = true;
	return true;
}

template<>
struct TStructOpsTypeTraits<FBouncingProjectileSpawnTargetData> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetDataHandle>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

UCLASS()
class ZORANSRESISTANCE_API AZoransTargetActor : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()
public:
	AZoransTargetActor(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void StartTargeting(UGameplayAbility* Ability) override;

	virtual void CancelTargeting() override;

	virtual void ConfirmTargeting() override;

	virtual void ConfirmTargetingAndContinue() override;

protected:
	EZoransTargetDataType ZoransTargetDataType = EZoransTargetDataType::None;

	UFUNCTION(BlueprintCallable, Category = "Zorans Target Data|Hitscan")
	void SetGenericTargetData();

	UFUNCTION(BlueprintCallable, Category = "Zorans Target Data|Hitscan")
	void SetHitscanResults(TArray<FHitscanData> InHits);
	
	// PROJECTILE SPAWNING
	UFUNCTION(BlueprintCallable, Category = "Zorans Target Data|Projectile")
	void SetBallisticProjectileSpawnParams(FBaseProjectileSpawnParams InBaseProjectileSpawnParams);

	UFUNCTION(BlueprintCallable, Category = "Zorans Target Data|Projectile")
	void SetBouncingProjectileSpawnParams(FBaseProjectileSpawnParams InBaseProjectileSpawnParams, TArray<FBouncingProjectilePathSegment> InBouncingProjectilePath);

	TArray<FHitscanData> HitScanResults;
	FBaseProjectileSpawnParams BaseProjectileSpawnParams;
	TArray<FBouncingProjectilePathSegment> BouncingProjectilePath;

	FGameplayAbilityTargetDataHandle MakeTargetDataFromGenericInput() const;
	FGameplayAbilityTargetDataHandle MakeTargetDataFromHitscanArray() const;
	FGameplayAbilityTargetDataHandle MakeTargetDataFromBallisticProjectileSpawnData() const;
	FGameplayAbilityTargetDataHandle MakeTargetDataFromBouncingProjectileSpawnData() const;
};
