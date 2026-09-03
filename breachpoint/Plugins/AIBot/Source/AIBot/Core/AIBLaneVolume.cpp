#include "Core/AIBLaneVolume.h"

#include "Components/BoxComponent.h"
#include "Core/AIBNavArea_Lane.h"
#include "Core/AIBRouteBias.h"
#include "NavModifierComponent.h"

AAIBLaneVolume::AAIBLaneVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// The modifier reads its bounds from registered, collision-enabled, nav-relevant
	// primitives on the owner (NavModifierComponent.cpp CalculateBounds); QueryOnly with
	// every channel ignored is the smallest shape that satisfies it without ever
	// touching a body.
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	RootComponent = Bounds;
	Bounds->InitBoxExtent(FVector(500.f, 500.f, 200.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bounds->SetGenerateOverlapEvents(false);
	Bounds->SetCanEverAffectNavigation(true);

	Modifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("LaneModifier"));
}

void AAIBLaneVolume::SetLane(int32 InLaneId, const FVector& HalfExtentUU)
{
	LaneId = FMath::Clamp(InLaneId, 1, AIB::MaxRouteLanes);
	Bounds->SetBoxExtent(HalfExtentUU);
	Modifier->SetAreaClass(AIBLanes::ClassOf(LaneId));
}

void AAIBLaneVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LaneId = FMath::Clamp(LaneId, 1, AIB::MaxRouteLanes);
	Modifier->SetAreaClass(AIBLanes::ClassOf(LaneId));
}
