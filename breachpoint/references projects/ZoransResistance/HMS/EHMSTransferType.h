// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
UENUM(BlueprintType)
enum class EHMSTransferType : uint8
{
	EGameNetDriverChunks,
	EGameNetDriverFull,
	ESteamNetDriver,
	ETCP
};
