#include "Execution/AIBStateTreeExecutor.h"

#include "AIBotModule.h"
#include "Components/StateTreeAIComponent.h"
#include "Core/AIBBotController.h"
#include "StateTree.h"

void UAIBStateTreeExecutor::Start(AAIBBotController& Bot)
{
	UStateTreeAIComponent* TreeComponent = Bot.GetStateTreeComponent();
	if (!TreeComponent)
	{
		UE_LOG(LogAIBot, Error, TEXT("AIBot: %s has no StateTree component — the executor cannot run."), *Bot.GetName());
		return;
	}

	// The tree arrives by ini soft path, set before logic starts (the host's proven,
	// respawn-idempotent shape). A miss is loud, once, and the bot stands (F7).
	if (UStateTree* Tree = Bot.GetBotStateTreePath().LoadSynchronous())
	{
		TreeComponent->SetStateTree(Tree);
	}
	else
	{
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: BotStateTree '%s' failed to load — %s will stand "
			"still. Check [/Script/AIBot.AIBBotController] against the asset (TICKET_AIB2 builds it)."),
			*Bot.GetBotStateTreePath().ToString(), *Bot.GetName());
		return;
	}

	Component = TreeComponent;
	TreeComponent->StartLogic();
}

void UAIBStateTreeExecutor::Stop()
{
	if (UStateTreeAIComponent* TreeComponent = Component.Get())
	{
		TreeComponent->StopLogic(TEXT("AIBot unpossessed"));
	}
	Component = nullptr;
}
