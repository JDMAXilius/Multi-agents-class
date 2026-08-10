#include "Match/BPGameMode.h"

#include "Character/BPCharacter.h"
#include "Match/BPPlayerController.h"
#include "Match/BPPlayerState.h"

ABPGameMode::ABPGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Fallback only. The pawn that is actually spawned comes from DefaultPawnClassOverride —
	// this bare C++ class has no SkeletalMesh assigned and is invisible in PIE.
	DefaultPawnClass = ABPCharacter::StaticClass();
	PlayerControllerClass = ABPPlayerController::StaticClass();
	PlayerStateClass = ABPPlayerState::StaticClass();
}

UClass* ABPGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (!DefaultPawnClassOverride.IsNull())
	{
		if (UClass* Overridden = DefaultPawnClassOverride.LoadSynchronous())
		{
			return Overridden;
		}

		UE_LOG(LogTemp, Error,
			TEXT("BPGameMode: DefaultPawnClassOverride '%s' failed to load — falling back to the meshless C++ pawn."),
			*DefaultPawnClassOverride.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("BPGameMode: DefaultPawnClassOverride is unset — pin it under [/Script/Breachpoint.BPGameMode] in DefaultGame.ini."));
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}
