// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WeaponData.generated.h"

class ANMWeaponActor;
class UNiagaraSystem;
class UReticleWidgetBase;
class USoundCue;
class USkeletalMeshComponent;
class ANMCharacterBase;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundCue;
class UAnimInstance;
class UAnimMontage;
class UDamageType;
class UCameraShakeBase;

UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	Single UMETA(DisplayName = "Single Fire"),
	Burst UMETA(DisplayName = "Burst Fire"),
	Automatic UMETA(DisplayName = "Automatic Fire")
};

USTRUCT(BlueprintType)
struct FWeaponData : public FTableRowBase
{
	GENERATED_BODY()

public:
	FWeaponData() 
		: MagazineCapacity(30)
		, BurstCount(1)
		, BaseDamage(10.f)
		, FireRate(300.f)
		, ReloadTime(2.5f)
		, BulletSpread(1.0f)
		, TimeBetweenShots(0.2f)
		, WeaponRange(10000.f)
	{}

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	FText WeaponDisplayName;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	USkeletalMesh* WeaponMesh = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	EWeaponFireMode FireMode;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	int32 MagazineCapacity;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	int32 BurstCount;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	float BaseDamage;

	// This is how many Hitscans the weapon will perform per shot. Useful for things like shotguns
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	int32 TracesPerShot = 1;

	// The radius of the Hitscan
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties", meta = (ClampMin=0.0f))
	float WeaponTraceDiameter = 2.0f;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	float FireRate;  // In rounds per minute

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	float ReloadTime;
	
	UPROPERTY(EditDefaultsOnly, Category = "Weapon | Properties", meta = (ClampMin=0.0f))
	float BulletSpread = 0.2f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	float TimeBetweenShots;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	float WeaponRange;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	float WeaponZoom;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Properties")
	bool bSniperZoom = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon | Properties")
	TObjectPtr<UNiagaraSystem> MuzzleEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon | Properties")
	TObjectPtr<UNiagaraSystem> HitImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon | Properties")
	TObjectPtr<UNiagaraSystem> TracerEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon | Properties")
	TObjectPtr<UNiagaraSystem> LasserEffect; 

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon | Properties")
	TObjectPtr<UNiagaraSystem> BulletEjectEffect;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | UI")
	TObjectPtr<UTexture2D> WeaponIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | UI")
	TObjectPtr<UTexture2D> AmmoIcon = nullptr;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | UI")
	TSubclassOf<UReticleWidgetBase> ReticleWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon | Character Audio")
	TObjectPtr<USoundCue> FireSound;
	
	UPROPERTY(EditDefaultsOnly, Category = "Weapon | Properties")
	TSubclassOf<UCameraShakeBase> FireCamShake;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Character Visuals")
	TSubclassOf<ANMWeaponActor> WeaponClass;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Character Visuals")
	TSubclassOf<UAnimInstance> CharacterAnimationClass = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Character Visuals")
	TObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Character Visuals")
	TObjectPtr<UAnimMontage> ReloadMontage;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Character Visuals")
	TObjectPtr<UAnimMontage> WeaponReloadMontage = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Character Visuals")
	TObjectPtr<UAnimMontage> WeaponFireMontage = nullptr;
};

USTRUCT(BlueprintType)
struct FTraceParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	bool bTraceComplex = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	FColor DebugColorOnHit = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	FColor DebugColorOnFail = FColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	float DebugDuration = 5.0f;
};

UENUM(BlueprintType)
enum class ETargetHitType : uint8
{
	None = 0,
	Static,
	Dynamic,
	Pawn
};

USTRUCT(BlueprintType)
struct FHitScanData
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "HitScan Data")
	AActor* HitActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "HitScan Data")
	AActor* SourceActor = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category = "HitScan Data")
	UPrimitiveComponent* HitComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "HitScan Data")
	FVector RelativeHitLocation = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadWrite, Category = "HitScan Data")
	FVector SurfaceNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "HitScan Data")
	FVector_NetQuantize TraceTo;

	UPROPERTY(BlueprintReadWrite, Category = "HitScan Data")
	ETargetHitType TargetHitType = ETargetHitType::None;

	UPROPERTY(BlueprintReadWrite, Category = "HitScan Data")
	TEnumAsByte<EPhysicalSurface> SurfaceType = EPhysicalSurface::SurfaceType_Default;
	
	UPROPERTY(BlueprintReadWrite, Category = "HitScan Data")
	FName BoneName = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "HitScan Data")
	float Radius = 0.0f;
};