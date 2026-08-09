// Copyright Zollpa LLC


#include "ZoransTeamSpawner.h"

#include "TeamStateFunctionLibrary.h"
#include "ZoransResistance/Characters/ZoransPlayerCharacter.h"
#include "ZoransResistance/HMS/FHMSActorReference.h"


AZoransTeamSpawner::AZoransTeamSpawner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

float AZoransTeamSpawner::GetSpawnScore() const
{
	const UWorld* World = GetWorld();
	float ClosestDistance = 100'000'000.f;
	for (TActorIterator<AZoransPlayerCharacter> It(World); It; ++It)
	{
		const AZoransPlayerCharacter* PlayerCharacter = *It;

		FVector PlayerLocation = PlayerCharacter->GetActorLocation();
		FVector SpawnerLocation = GetActorLocation();
		const float Distance = FVector::DistSquared(PlayerLocation, SpawnerLocation);
		
		ETeamComparisonResult ComparisonResult;
		UTeamStateFunctionLibrary::GetTeamDispositionByIndex(this, TeamIndex, PlayerCharacter->GetTeamAssignment(), ComparisonResult);
		if(ComparisonResult == ETeamComparisonResult::Hostile && Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
		}
	}
	return ClosestDistance;
}

