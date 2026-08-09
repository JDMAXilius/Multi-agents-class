// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NMGameModeData.generated.h"

UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	Waiting			UMETA(DisplayName = "Waiting for Players"),
	Warmup			UMETA(DisplayName = "Warmup"),
	InProgress		UMETA(DisplayName = "In Progress"),
	Cooldown		UMETA(DisplayName = "Cooldown"),
	Ended			UMETA(DisplayName = "Ended"),
	Loading			UMETA(DisplayName = "Loading"),
	ServerReady		UMETA(DisplayName = "ServerReady")
};

USTRUCT(BlueprintType)
struct FGameModeDetails
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	float WaitingTimeLimit = 5.0f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	float WarmupTimeLimit = 1.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	float RoundTimeLimit = 15.0f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	float CooldownTimeLimit = 1.0f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	int32 ScoreLimit = 0;
};