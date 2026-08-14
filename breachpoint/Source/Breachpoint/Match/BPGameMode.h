#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "BPGameMode.generated.h"

/**
 * Minimal GameMode scaffold (BP prefix — starter / experiment).
 * Spawns ABPCharacter / ABPPlayerController / ABPPlayerState.
 * Not the production match frame; that remains ABRGameMode.
 */
UCLASS(config = Game, meta = (DisplayName = "BP Game Mode"))
class BREACHPOINT_API ABPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ABPGameMode();

protected:

	/**
	 * The pawn actually spawned. Pinned under [/Script/Breachpoint.BPGameMode] in DefaultGame.ini.
	 *
	 * ABPCharacter creates both mesh components but assigns no SkeletalMesh — a hard ref there
	 * would violate law 3. The meshes live on the R26 BP child /Game/Characters/BP, so spawning
	 * the C++ class directly gives an invisible pawn with its camera on a socket that does not
	 * exist. Soft, so this stays a data reference (contract_gap BP82-9).
	 */
	//UPROPERTY(Config, EditDefaultsOnly, Category = "Classes")
	//TSoftClassPtr<APawn> DefaultPawnClassOverride;

	//virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
};
