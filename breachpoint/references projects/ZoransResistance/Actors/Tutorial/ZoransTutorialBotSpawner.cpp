// Copyright Zollpa LLC


#include "ZoransTutorialBotSpawner.h"

#include "ZoransTutorialController.h"
#include "ZoransResistance/HMS/FHMSActorReference.h"


AZoransTutorialBotSpawner::AZoransTutorialBotSpawner(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	TeamIndex = -1;
}

void AZoransTutorialBotSpawner::BeginPlay()
{
	Super::BeginPlay();
	if(HasAuthority())
	{
		const UWorld* World = GetWorld();
		for (const TActorIterator<AZoransTutorialController> It(World, AZoransTutorialController::StaticClass()); It;)
		{
			const AZoransTutorialController* TutorialController = *It;
			TutorialController->AddTutorialBotSpawner(this);
			break;
		}
	}
}

