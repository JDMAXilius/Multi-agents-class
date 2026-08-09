// Copyright Zollpa LLC


#include "ZoransResistance/HMS/FHMSActorReference.h"
#include "ZoransResistance/HMS/HMSSavedStateComponent.h"

AActor* FHMSActorReference::GetReference(UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	if (!ActorRef->IsValidLowLevelFast() || ActorRef->IsPendingKillOrUnreachable() || ActorRef->IsPendingKill() || ActorRef->StaticClass() != ActorClass)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(WorldContextObject->GetWorld(), ActorClass, FoundActors);

		for (AActor* actor : FoundActors)
		{
			UHMSSavedStateComponent* r = Cast<UHMSSavedStateComponent>(actor->GetComponentByClass(UHMSSavedStateComponent::StaticClass()));
			if (!r) continue;

			if (r->Tag == ActorID)
			{
				ActorRef = actor;
				return ActorRef;

			}
		}

		return ActorRef;
	}
	else
		return ActorRef;
}
