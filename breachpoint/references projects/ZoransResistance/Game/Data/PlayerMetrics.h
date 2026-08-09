// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "PlayerMetrics.generated.h"

UENUM(BlueprintType)
enum class EWeaponMetric : uint8
{
	None				UMETA(DisplayName = "None"),
	// Amount of shots fired with this weapon
	Shots				UMETA(DisplayName = "Shots"),
	// Total number of hits with this weapon
	Hits				UMETA(DisplayName = "Hits"),
	// Hits that had a damage modifier > 0.f
	WeakPoint			UMETA(DisplayName = "WeakPoint"),
	// Amount of kills with this weapon
	Kills				UMETA(DisplayName = "Kills"),
	// Melee kills with this weapon
	Melee				UMETA(DisplayName = "Melee"),
	// Total number of damage dealt with this weapon
	Damage				UMETA(DisplayName = "Damage"),
	// Total number of health recovered by this weapon
	Healing				UMETA(DisplayName = "Healing"),
	// Amount of damage prevented by this weapon
	Mitigation			UMETA(DisplayName = "Mitigation")
};

UENUM(BlueprintType)
enum class EAbilityMetric : uint8
{
	None				UMETA(DisplayName = "None"),
	// Amount of times this ability has been activated
	Activations			UMETA(DisplayName = "Activations"),
	// Amount of kills with this ability
	Kills				UMETA(DisplayName = "Kills"),
	// Total amount of damage dealt by this ability
	Damage				UMETA(DisplayName = "Damage"),
	// Total amount of health recovered by this weapon
	Healing				UMETA(DisplayName = "Healed"),
	// Total amount of time this ability has been active
	Duration			UMETA(DisplayName = "Duration"),
	// Amount of damage prevented by this ability
	Mitigation			UMETA(DisplayName = "Mitigation")
};

USTRUCT(BlueprintType)
struct FWeaponMetrics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<EWeaponMetric, float> Metrics;
};

USTRUCT(BlueprintType)
struct FAbilityMetrics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<EAbilityMetric, float> Metrics;
};

USTRUCT(BlueprintType)
struct FPlayerMetrics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FName, FWeaponMetrics> WeaponMetrics;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FName, FAbilityMetrics> AbilityMetrics;
};