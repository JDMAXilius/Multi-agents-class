#include "AI/BNPointOfInterest.h"

#include "Components/SceneComponent.h"

ABNPointOfInterest::ABNPointOfInterest()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
