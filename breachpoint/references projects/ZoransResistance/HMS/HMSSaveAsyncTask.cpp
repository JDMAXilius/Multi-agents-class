// Copyright Zollpa LLC


#include "ZoransResistance/HMS/HMSSaveAsyncTask.h"
#include "ZoransResistance/HMS/HMSSavedStateComponent.h"
#include "ZoransResistance/HMS/FHMSActorState.h"
#include "ZoransResistance/HMS/HMSGMComponent.h"

void SaveAsyncTask::DoWork()
{
	TArray<FHMSActorState> ActorSaves;



	for (const AActor* actor : *LevelActors)
	{
		const UHMSSavedStateComponent* comp = actor->FindComponentByClass<UHMSSavedStateComponent>();
		if (comp)
		{

			if (comp->Enabled)
			{
				const APawn* pawn = Cast<APawn>(actor);
				const APlayerController* pc = nullptr;
				if (pawn)
					pc = Cast<APlayerController>(pawn->GetController());

				if (pc)
				{
					if (pc->IsLocalController())
					{
						if (!IgnoreHostPawn)
							ActorSaves.Add(comp->SaveState());
					}
					else
					{
						ActorSaves.Add(comp->SaveState());
					}
				}
				else
					ActorSaves.Add(comp->SaveState());
			}
		}
	}

	if (GameMode)
	{
		if (GameMode->FindComponentByClass<UHMSGMComponent>())
		{
			GameMode->FindComponentByClass<UHMSGMComponent>()->OnActorsSaved.Broadcast(ActorSaves);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GAME MODE INVALID"));
		}
	}

		
}
