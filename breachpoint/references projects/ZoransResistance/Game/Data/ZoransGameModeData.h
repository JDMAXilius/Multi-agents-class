// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ZoransResistance/Player/Data/LocalPlayerData.h"
#include "ZoransGameModeData.generated.h"

class UZoransAttributeSet;
class UZoransGameplayEffect;
class UZoransGameplayAbility;

UENUM(BlueprintType, Meta = (DisplayName = "Game Mode Override"))
enum class EGameModeOverride : uint8
{
	Default					UMETA(DisplayName = "Default"),
	Tutorial				UMETA(DisplayName = "Tutorial"),
	TeamDeathMatch			UMETA(DisplayName = "Team Deathmatch"),
	ControlPoint			UMETA(DisplayName = "Control Point"),
	GunGame					UMETA(DisplayName = "Gun Game"),
	DataDash				UMETA(DisplayName = "Data Dash"),
};

static TMap<FString, EGameModeOverride> GameModeMap =
{
	{"Default", EGameModeOverride::Default},
	{"", EGameModeOverride::Default},

	{"Tutorial", EGameModeOverride::Tutorial},
	
	{"Team DeathMatch", EGameModeOverride::TeamDeathMatch},
	{"TeamDeathMatch", EGameModeOverride::TeamDeathMatch},
	
	{"Control Point", EGameModeOverride::ControlPoint},
	{"ControlPoint", EGameModeOverride::ControlPoint},
	
	{"Gun Game", EGameModeOverride::GunGame},
	{"GunGame", EGameModeOverride::GunGame},
	
	{"Data Dash", EGameModeOverride::DataDash},
	{"DataDash", EGameModeOverride::DataDash},
	{"Data Dash (CTF)", EGameModeOverride::DataDash}
};

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
	int32 MaximumTeamSize = 8;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	int32 MinimumTeamSize = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	float WaitingTimeLimit = 5.0f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	float WarmupTimeLimit = 1.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	float RoundTimeLimit = 15.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	float PauseTimeLimit = 10.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	float CooldownTimeLimit = 1.0f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Mode Details")
	int32 ScoreLimit = 0;
};

USTRUCT(BlueprintType)
struct FBotData : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Details")
	TObjectPtr<UTexture2D> ThumbnailIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Bot Loadout")
	FPlayerLoadout BotLoadout1 = FPlayerLoadout();

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Bot Loadout")
	FPlayerLoadout BotLoadout2 = FPlayerLoadout();

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Bot Loadout")
	FPlayerLoadout BotLoadout3 = FPlayerLoadout();
};