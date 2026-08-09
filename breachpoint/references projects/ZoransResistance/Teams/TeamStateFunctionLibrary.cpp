// Copyright Zollpa LLC.


#include "TeamStateFunctionLibrary.h"
#include "TeamStateInterface.h"
#include "ZoransResistance/Game/ZoransGameState.h"
#include "ZoransTeamObject.h"
#include "ZoransResistance/Game/ZoransGameModeLogic.h"

void UTeamStateFunctionLibrary::GetTeamDispositionFromActors(const UObject* WorldContextObject, AActor* FirstActor, AActor* SecondActor, ETeamComparisonResult& Result)
{
	Result = ETeamComparisonResult::Invalid;
		
	if (IsValid(FirstActor) && IsValid(SecondActor))
	{
		int32 FirstTeam;
		int32 SecondTeam;
		
		//Check if free-for-all
		if(const AZoransGameState* ZoransGameState = Cast<AZoransGameState>(WorldContextObject->GetWorld()->GetGameState()))
		{
			if(ZoransGameState->GetGameModeSettings().bFreeForAll)
			{
				Result = ETeamComparisonResult::Hostile;
				return;
			}
		}
		
		if (const ITeamStateInterface* TeamStateInterface1a = Cast<ITeamStateInterface>(FirstActor))
		{
			FirstTeam = TeamStateInterface1a->GetTeamAssignment();

			if (FirstTeam < 0) return;
		}
		else if (const ITeamStateInterface* TeamStateInterface1b = Cast<ITeamStateInterface>(FirstActor->GetOwner()))
		{
			FirstTeam = TeamStateInterface1b->GetTeamAssignment();

			if (FirstTeam < 0) return;
		}
		else
		{
			return;
		}

		if (const ITeamStateInterface* TeamStateInterface2a = Cast<ITeamStateInterface>(SecondActor))
		{
			SecondTeam = TeamStateInterface2a->GetTeamAssignment();

			if (SecondTeam < 0) return;
		}
		else if (const ITeamStateInterface* TeamStateInterface2b = Cast<ITeamStateInterface>(SecondActor->GetOwner()))
		{
			SecondTeam = TeamStateInterface2b->GetTeamAssignment();

			if (SecondTeam < 0) return;
		}
		else
		{
			return;
		}

		if (FirstTeam == SecondTeam)
		{
			Result = ETeamComparisonResult::Friendly;

			return;
		}

		if (const AZoransGameState* ZoransGameState = Cast<AZoransGameState>(WorldContextObject->GetWorld()->GetGameState()))
		{
			if (const UZoransTeamObject* FirstTeamObject = ZoransGameState->GetTeamObjectByTeamIndex(FirstTeam))
			{
				if (const TArray<int32> FirstTeamAllies = FirstTeamObject->FriendlyTeams; FirstTeamAllies.Contains(SecondTeam))
				{
					Result = ETeamComparisonResult::Friendly;

					return;
				}
			}

			if (const UZoransTeamObject* SecondTeamObject = ZoransGameState->GetTeamObjectByTeamIndex(SecondTeam))
			{
				if (const TArray<int32> SecondTeamAllies = SecondTeamObject->FriendlyTeams; SecondTeamAllies.Contains(FirstTeam))
				{
					Result = ETeamComparisonResult::Friendly;

					return;
				}
			}
		}
	}

	//@todo figure out what this... actually means?
	Result = ETeamComparisonResult::Hostile;
}

void UTeamStateFunctionLibrary::GetTeamDispositionByIndex(const UObject* WorldContextObject, const int32 FirstIndex, const int32 SecondIndex, ETeamComparisonResult& Result)
{
	Result = ETeamComparisonResult::Invalid;

	//Check if free-for-all
	if(const AZoransGameState* ZoransGameState = Cast<AZoransGameState>(WorldContextObject->GetWorld()->GetGameState()))
	{
		if(ZoransGameState->GetGameModeSettings().bFreeForAll)
		{
			Result = ETeamComparisonResult::Hostile;
			return;
		}
	}

	if (FirstIndex < 0 || SecondIndex < 0)
	{
		return;
	}

	if (FirstIndex == SecondIndex)
	{
		Result = ETeamComparisonResult::Friendly;

		return;
	}

	if (const AZoransGameState* ZoransGameState = Cast<AZoransGameState>(WorldContextObject->GetWorld()->GetGameState()))
	{
		if (const UZoransTeamObject* FirstTeamObject = ZoransGameState->GetTeamObjectByTeamIndex(FirstIndex))
		{
			if (const TArray<int32> FirstTeamAllies = FirstTeamObject->FriendlyTeams; FirstTeamAllies.Contains(SecondIndex))
			{
				Result = ETeamComparisonResult::Friendly;

				return;
			}
		}

		if (const UZoransTeamObject* SecondTeamObject = ZoransGameState->GetTeamObjectByTeamIndex(SecondIndex))
		{
			if (const TArray<int32> SecondTeamAllies = SecondTeamObject->FriendlyTeams; SecondTeamAllies.Contains(FirstIndex))
			{
				Result = ETeamComparisonResult::Friendly;

				return;
			}
		}
		
		Result = ETeamComparisonResult::Hostile;
	}
}

UZoransTeamObject* UTeamStateFunctionLibrary::GetLowestPopulationTeam(const UObject* WorldContextObject)
{
	UZoransTeamObject* LowestTeam = nullptr;
	
	if (AZoransGameState* ZoransGameState = Cast<AZoransGameState>(WorldContextObject->GetWorld()->GetGameState()))
	{
		LowestTeam = ZoransGameState->GetLowestPopulationTeam();
	}
	
	return LowestTeam;
}
