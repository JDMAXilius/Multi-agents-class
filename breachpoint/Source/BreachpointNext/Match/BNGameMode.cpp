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

void ABNGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (UClass* PawnClass = DefaultPawnClassPath.TryLoadClass<APawn>())
	{
		DefaultPawnClass = PawnClass;
	}
}
