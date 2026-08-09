// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "TeamData.generated.h"

class AZoransPlayerState;

UENUM(BlueprintType)
enum class EStatChangeOperation : uint8
{
	Add,
	Subtract,
	Multiply,
	Divide,
	Override
};

UENUM(BlueprintType)
enum class EScoreStat : uint8
{
	Kills,
	Deaths,
	Assists,
	Objective
};

USTRUCT(BlueprintType)
struct FScoreStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Assists = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Objective = 0;
};

USTRUCT(BlueprintType)
struct FTeamDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FString TeamName = "Team Name";

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FColor TeamColor = FColor::Silver;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	bool bFreeForAllTeam = false;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TArray<int32> FriendlyTeams;
};

UENUM(BlueprintType)
enum class ETeamComparisonResult : uint8
{
	Friendly,
	Hostile,
	Invalid
};