#include "Match/BNGameMode.h"
#include "Characters/BNCharacter.h"
#include "Match/BNPlayerController.h"
#include "Match/BNPlayerState.h"

ABNGameMode::ABNGameMode()
{
	DefaultPawnClass = ABNCharacter::StaticClass();
	PlayerControllerClass = ABNPlayerController::StaticClass();
	PlayerStateClass = ABNPlayerState::StaticClass();
}
