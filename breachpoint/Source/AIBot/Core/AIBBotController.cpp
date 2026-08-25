#include "Core/AIBBotController.h"

#include "AIBotModule.h"
#include "Components/ActorComponent.h"
#include "Interfaces/AIBAvatarInterface.h"

AAIBBotController::AAIBBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A real PlayerState: the bot joins the same match machinery a human does —
	// scoreboard, killfeed, respawn stamps all work with no bot special case.
	bWantsPlayerState = true;

	// Law 4: no gameplay Tick. Thinking is timer-driven; reacting is stimulus-driven.
	PrimaryActorTick.bCanEverTick = false;
}

void AAIBBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	Avatar = nullptr;
	AvatarObject = nullptr;

	if (!InPawn)
	{
		return;
	}

	// The one lookup that joins bot to body: any component on the pawn implementing the
	// avatar interface. The game's adapter supplies it; this module never knows its type.
	for (UActorComponent* Component : InPawn->GetComponents())
	{
		if (IAIBAvatarInterface* AsAvatar = Cast<IAIBAvatarInterface>(Component))
		{
			Avatar = AsAvatar;
			AvatarObject = Component;
			break;
		}
	}

	if (Avatar)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s possessed %s, avatar door open."),
			*GetName(), *InPawn->GetName());
	}
	else
	{
		// The designed miss answer: loud, once, and the bot stands still — a missing
		// adapter must never read as a mysterious silent bot.
		UE_LOG(LogAIBot, Error, TEXT("AIBot: %s possessed %s but found NO avatar adapter "
			"component. The bot will stand still. The game must add its adapter to the "
			"pawn (see Interfaces/AIBAvatarInterface.h)."),
			*GetName(), *InPawn->GetName());
	}
}

void AAIBBotController::OnUnPossess()
{
	Avatar = nullptr;
	AvatarObject = nullptr;
	Super::OnUnPossess();
}
