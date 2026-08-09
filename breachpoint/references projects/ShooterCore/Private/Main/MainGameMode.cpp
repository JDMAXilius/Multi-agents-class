
#include "Main/MainGameMode.h"
#include "Main/PlayerCharacter.h"
#include "Main/MainPlayerController.h"

AMainGameMode::AMainGameMode()
{
	DefaultPawnClass = APlayerCharacter::StaticClass();
	PlayerControllerClass = AMainPlayerController::StaticClass();
}
