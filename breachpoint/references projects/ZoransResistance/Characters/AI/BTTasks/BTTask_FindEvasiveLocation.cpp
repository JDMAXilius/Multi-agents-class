// Copyright Zollpa LLC


#include "BTTask_FindEvasiveLocation.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "ZoransResistance/Player/ZoransAIController.h"

UBTTask_FindEvasiveLocation::UBTTask_FindEvasiveLocation(const FObjectInitializer& ObjectInitializer)
{
	bCreateNodeInstance = true;
	NodeName = "Find Evasive Location";
	
}

EBTNodeResult::Type UBTTask_FindEvasiveLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	//return Super::ExecuteTask(OwnerComp, NodeMemory);
	Cntrlr = Cast<AZoransAIController>(OwnerComp.GetAIOwner());

	switch(const AIEngageRange TargetE = static_cast<AIEngageRange>(Cntrlr->BBC->GetValueAsEnum("Engagement Range")))
	{
		
	case AIEngageRange::None:
		
		break;
	
	case AIEngageRange::Close:
		
		LocationSeekerQuery = LocationSeekerQuery_Close;
		break;
		
	case AIEngageRange::Close_Medium:
		
		LocationSeekerQuery = LocationSeekerQuery_Close_Medium;
		break;
		
	case AIEngageRange::Medium:
		
		LocationSeekerQuery = LocationSeekerQuery_Medium;
		break;
		
	case AIEngageRange::Medium_Far:
		
		LocationSeekerQuery = LocationSeekerQuery_Medium_Far;
		break;
		
	case AIEngageRange::Far:
		
		LocationSeekerQuery = LocationSeekerQuery_Far;
		break;
		
	default:
		
		LocationSeekerQuery = LocationSeekerQuery_Close;
		break;
	}
	if (Cntrlr && LocationSeekerQuery)
	{
		LocationSeekerQueryRequest = FEnvQueryRequest(LocationSeekerQuery, Cntrlr->GetOwningAI());
		LocationSeekerQueryRequest.Execute(EEnvQueryRunMode::RandomBest5Pct, this, &UBTTask_FindEvasiveLocation::LocationSeekerQueryFinished);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
	
}

void UBTTask_FindEvasiveLocation::LocationSeekerQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	int32 Index = 0;
	float CurrentBestScore = 0;
	TArray<FVector> Locations;
	Result->GetAllAsLocations(Locations);

	for (auto& Loc : Locations)
	{
		if (IsDistanceGreaterThanX(Loc) && Result->GetItemScore(Index) > CurrentBestScore)
		{
			EvasiveLocation = Loc;
			CurrentBestScore = Result->GetItemScore(Index);
		}

		Index++;
	}

	Cntrlr->BBC->SetValueAsVector("TargetLocation", EvasiveLocation);
}

bool UBTTask_FindEvasiveLocation::IsDistanceGreaterThanX(FVector Location)
{

	bool ConsiderLocation = true;
	const AActor* target = Cast<AActor>(Cntrlr->BBC->GetValueAsObject("TargetEnemy"));
	if (target)
	{
		const float CalculatedDistance = UKismetMathLibrary::Vector_Distance(Location,target->GetActorLocation());
		if (CalculatedDistance <= Distance)
		{
			ConsiderLocation = false;
		}
	}
	return ConsiderLocation;
}
