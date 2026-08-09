#pragma once
// Session name constant, enums, and host match settings for multiplayer.

#include "CoreMinimal.h"
#include "OSSessionsData.generated.h"

/** Custom session name for OnSight game sessions */
#define OnSightGameSession FName("OnSightGameSession")

// --- Join Result ---

/**
 * Blueprint-friendly enum for join session results
 * Maps from EOnJoinSessionCompleteResult::Type to a Blueprint-exposed enum
 */
UENUM(BlueprintType)
enum class OSJoinSessionResultType : uint8
{
	/** The join worked as expected */
	Success					UMETA(DisplayName = "Success"),
	/** There are no open slots to join */
	SessionIsFull			UMETA(DisplayName = "Session Is Full"),
	/** The session couldn't be found on the service */
	SessionDoesNotExist		UMETA(DisplayName = "Session Does Not Exist"),
	/** There was an error getting the session server's address */
	CouldNotRetrieveAddress UMETA(DisplayName = "Could Not Retrieve Address"),
	/** The user attempting to join is already a member of the session */
	AlreadyInSession		UMETA(DisplayName = "Already In Session"),
	/** An error not covered above occurred */
	UnknownError			UMETA(DisplayName = "Unknown Error")
};

// --- Game Mode ---

UENUM(BlueprintType)
enum class EOSGameModeType : uint8
{
	DeathMatch		UMETA(DisplayName = "FFA"),
	TeamDeathMatch	UMETA(DisplayName = "TDM"),
	Domination		UMETA(DisplayName = "Domination")
};

// --- Player Count ---

UENUM(BlueprintType)
enum class EOSPlayerCount : uint8
{
	None		= 0		UMETA(Hidden),
	Players_2	= 2		UMETA(DisplayName = "2 Players"),
	Players_4	= 4		UMETA(DisplayName = "4 Players"),
	Players_6	= 6		UMETA(DisplayName = "6 Players"),
	Players_8	= 8		UMETA(DisplayName = "8 Players"),
	Players_40	= 40	UMETA(DisplayName = "40 Players (Dev)")
};

// --- Host Match Settings ---

USTRUCT(BlueprintType)
struct FOSHostMatchSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
	FString MapPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
	EOSGameModeType GameMode = EOSGameModeType::DeathMatch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
	EOSPlayerCount PlayerCount = EOSPlayerCount::Players_8;

	// Convenience: get the MatchType string for session filtering
	FString GetMatchTypeString() const
	{
		switch (GameMode)
		{
		case EOSGameModeType::DeathMatch:		return TEXT("DeathMatch");
		case EOSGameModeType::TeamDeathMatch:	return TEXT("TeamDeathMatch");
		case EOSGameModeType::Domination:		return TEXT("Domination");
		default:								return TEXT("DeathMatch");
		}
	}

	// Convenience: get the player count as int32
	int32 GetPlayerCount() const
	{
		return static_cast<int32>(PlayerCount);
	}
};
