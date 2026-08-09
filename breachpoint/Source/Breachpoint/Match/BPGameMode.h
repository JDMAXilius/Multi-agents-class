#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "BPGameMode.generated.h"

/**
 * Minimal GameMode scaffold (BP prefix — starter / experiment).
 * Spawns ABPCharacter / ABPPlayerController / ABPPlayerState.
 * Not the production match frame; that remains ABRGameMode.
 */
UCLASS(meta = (DisplayName = "BP Game Mode"))
class BREACHPOINT_API ABPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ABPGameMode();
};
