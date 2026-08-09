// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "AbilitySystemData.generated.h"

class AZoransThrowdownBallisticBase;
class UZoransAttributeSet;
class UZoransGameplayEffect;
class UZoransGameplayAbility;
class UNiagaraSystem;
class USoundCue;

UENUM(BlueprintType)
enum class EAbilityBindings : uint8
{
	None,
	ConfirmTarget,
	CancelTarget,
	Interact,
	PrimaryAction,
	SecondaryAction,
	OffensiveAction,
	JumpAction,
	MobilityAction,
	ClassAbility1,
	ClassAbility2,
	ClassAbility3,
	ClassAbility4
};

UENUM(BlueprintType)
enum class ETargetHitType : uint8
{
	None,
	Static,
	Dynamic,
	Pawn
};

UENUM(BlueprintType)
enum class EHitSeverity : uint8
{
	Default,
	WeakPoint,
	Fatal,
	FatalWeakPoint,
	Throwdown
};

USTRUCT(BlueprintType)
struct FExplosionDamageModifiers
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Explosion Damage")
	AActor* HitActor = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Explosion Damage")
	float TotalModifier = 1.f;
};

USTRUCT(BlueprintType)
struct FDamageMetricValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability Metrics")
	float Damage = 0.f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability Metrics")
	bool bWeakPoint = false;
};

USTRUCT(BlueprintType)
struct FGrantedAbilitySystemData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TArray<TSubclassOf<UZoransAttributeSet>> GrantedAttributes;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TArray<TSubclassOf<UZoransGameplayEffect>> GrantedGameplayEffects;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TArray<TSubclassOf<UZoransGameplayAbility>> GrantedAbilities;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	FGameplayTagContainer GrantedGameplayTags;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TSubclassOf<UZoransGameplayAbility> MobilityAction;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TSubclassOf<UZoransGameplayAbility> OffensiveAction;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TSubclassOf<UZoransGameplayAbility> JumpAction;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TSubclassOf<UZoransGameplayAbility> ClassAbility1;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TSubclassOf<UZoransGameplayAbility> ClassAbility2;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TSubclassOf<UZoransGameplayAbility> ClassAbility3;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TSubclassOf<UZoransGameplayAbility> ClassAbility4;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Details")
	FDataTableRowHandle WeaponRowHandle;
};

USTRUCT(BlueprintType)
struct FHitscanData
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Data")
	AActor* HitActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Data")
	AActor* SourceActor = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Data")
	UPrimitiveComponent* HitComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Data")
	FVector RelativeHitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Data")
	FVector SurfaceNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Data")
	ETargetHitType TargetHitType = ETargetHitType::None;

	UPROPERTY(BlueprintReadWrite, Category = "Hitscan Data")
	FName BoneName = NAME_None;
};

USTRUCT(BlueprintType)
struct FThrowdownBallisticData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Throwdown Ballistic")
	TObjectPtr<UAnimMontage> AbilityMontage = nullptr;

	// THIS MUST BE A CHILD OF BP_THROWDOWN_BALLISTIC_BASE AND HAVE NO MODIFICATIONS TO DAMAGE OR ATTRIBUTES, COSMETIC CHANGES ONLY
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Throwdown Ballistic")
	TSubclassOf<AZoransThrowdownBallisticBase> ProjectileOverride = nullptr;
};

USTRUCT(BlueprintType)
struct FThrowdownBeamData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Throwdown Beam")
	TObjectPtr<UAnimMontage> AbilityMontage = nullptr;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Throwdown Beam")
	TObjectPtr<UNiagaraSystem> BeamParticle = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Throwdown Beam")
	TObjectPtr<USoundCue> SourceSound = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Throwdown Beam")
	TObjectPtr<USoundCue> BeamSound = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Throwdown Beam")
	TObjectPtr<USoundCue> ImpactSound = nullptr;
};