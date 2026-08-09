#pragma once

#include "CoreMinimal.h"
#include "OSTDMTeamData.generated.h"

/*
 Match configuration for TDM.
 Time limit is set via the base MatchDurationSeconds on OSGameMode.
 Kill limit and time limit are "whichever first" end conditions.
*/
USTRUCT(BlueprintType)
struct FOSTDMMatchConfig
{
	GENERATED_BODY()

	/** First team to reach this many kills wins. 0 = disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OnSight|Match|TDM")
	int32 KillLimit = 30;
};
