#include "TDM/OSTDMPlayerStart.h"

#include "Core/OSPlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

AOSTDMPlayerStart::AOSTDMPlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

float AOSTDMPlayerStart::GetSpawnScore(AController* RequestingController) const
{
	const UWorld* World = GetWorld();
	if (!World || !IsValid(RequestingController))
	{
		return 0.f;
	}

	const AOSPlayerState* RequestPS = RequestingController->GetPlayerState<AOSPlayerState>();
	const int32 RequestTeam = RequestPS ? RequestPS->GetTeamIndex() : -1;

	const FVector SpawnLoc = GetActorLocation();
	float ClosestHostileDistSq = TNumericLimits<float>::Max();

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AController* OtherController = It->Get();
		if (!IsValid(OtherController) || OtherController == RequestingController)
		{
			continue;
		}

		const APawn* OtherPawn = OtherController->GetPawn();
		const AOSPlayerState* OtherPS = OtherController->GetPlayerState<AOSPlayerState>();
		if (!IsValid(OtherPawn) || !IsValid(OtherPS))
		{
			continue;
		}

		const int32 OtherTeam = OtherPS->GetTeamIndex();
		const bool bHostile = (RequestTeam >= 0 && OtherTeam >= 0 && RequestTeam != OtherTeam);
		if (!bHostile)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(SpawnLoc, OtherPawn->GetActorLocation());
		if (DistSq < ClosestHostileDistSq)
		{
			ClosestHostileDistSq = DistSq;
		}
	}

	return ClosestHostileDistSq == TNumericLimits<float>::Max() ? BIG_NUMBER : ClosestHostileDistSq;
}
