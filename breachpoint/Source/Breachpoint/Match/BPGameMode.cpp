#include "Match/BPGameMode.h"

#include "Character/BPCharacter.h"
#include "Match/BPPlayerController.h"
#include "Match/BPPlayerState.h"

ABPGameMode::ABPGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	DefaultPawnClass = ABPCharacter::StaticClass();
	PlayerControllerClass = ABPPlayerController::StaticClass();
	PlayerStateClass = ABPPlayerState::StaticClass();
}
