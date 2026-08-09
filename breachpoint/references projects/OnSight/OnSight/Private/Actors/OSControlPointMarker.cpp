#include "Actors/OSControlPointMarker.h"
#include "Actors/OSControlPoint.h"
#include "Core/GameModes/OSGameMode_Domination.h"

AOSControlPointMarker::AOSControlPointMarker()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = false;
	// ControlPointClass defaults to nullptr — designer MUST set BP_OSControlPoint in Blueprint.
}

void AOSControlPointMarker::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) return;

	// Only spawn control points in Domination mode
	AOSGameMode_Domination* DomGM = Cast<AOSGameMode_Domination>(GetWorld()->GetAuthGameMode());
	if (!DomGM || !ControlPointClass) return;

	SpawnedPoint = GetWorld()->SpawnActorDeferred<AOSControlPoint>(
		ControlPointClass, GetActorTransform(), this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (SpawnedPoint)
	{
		// PointLabel is protected — use public setter. Safe before FinishSpawning.
		SpawnedPoint->SetPointLabel(PointLabel);
		SpawnedPoint->FinishSpawning(GetActorTransform());
	}
}

void AOSControlPointMarker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up spawned control point to prevent orphans on level unload.
	// The control point's own EndPlay -> RequestUnregister handles GM deregistration.
	if (IsValid(SpawnedPoint))
	{
		SpawnedPoint->Destroy();
		SpawnedPoint = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
