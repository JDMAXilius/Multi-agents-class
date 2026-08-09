
#include "Actors/GrenadeSplineEndPointBase.h"
#include "Components/StaticMeshComponent.h"

AGrenadeSplineEndPointBase::AGrenadeSplineEndPointBase()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(FName("BlockAllDynamic"), true);
}

void AGrenadeSplineEndPointBase::BeginPlay()
{
	Super::BeginPlay();	
}

void AGrenadeSplineEndPointBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

