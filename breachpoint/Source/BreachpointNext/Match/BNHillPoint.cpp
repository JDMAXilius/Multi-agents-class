#include "Match/BNHillPoint.h"

#include "Components/SceneComponent.h"

ABNHillPoint::ABNHillPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	// A real root, or the actor cannot hold a position — the AIB1 fixture lesson,
	// applied before it can bite a shipping actor.
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
