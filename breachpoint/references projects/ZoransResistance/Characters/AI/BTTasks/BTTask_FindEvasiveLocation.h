// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryManager.h"
//#include "ZoransAIController.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_FindEvasiveLocation.generated.h"

/**
 * 
 */
class AZoransAIController;

UCLASS()
class ZORANSRESISTANCE_API UBTTask_FindEvasiveLocation : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	
	UBTTask_FindEvasiveLocation(const FObjectInitializer& ObjectInitializer);

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY()
	UEnvQuery* LocationSeekerQuery; // set the query in editor
	UPROPERTY(EditAnyWhere, BlueprintReadWrite, Category = "Blackboard")
	UEnvQuery* LocationSeekerQuery_Close; // set the query in editor
	UPROPERTY(EditAnyWhere, BlueprintReadWrite, Category = "Blackboard")
	UEnvQuery* LocationSeekerQuery_Close_Medium; // set the query in editor
	UPROPERTY(EditAnyWhere, BlueprintReadWrite, Category = "Blackboard")
	UEnvQuery* LocationSeekerQuery_Medium; // set the query in editor
	UPROPERTY(EditAnyWhere, BlueprintReadWrite, Category = "Blackboard")
	UEnvQuery* LocationSeekerQuery_Medium_Far; // set the query in editor
	UPROPERTY(EditAnyWhere, BlueprintReadWrite, Category = "Blackboard")
	UEnvQuery* LocationSeekerQuery_Far; // set the query in editor

	//UPROPERTY(EditAnyWhere, BlueprintReadWrite, Category = "Blackboard")
	//EEnvQueryRunMode RunMode;
	

	FEnvQueryRequest LocationSeekerQueryRequest;

	// The function that gets called when querry finished
	void LocationSeekerQueryFinished(TSharedPtr<FEnvQueryResult> Result);

	TObjectPtr<AZoransAIController> Cntrlr;

	FVector EvasiveLocation = FVector::ZeroVector;

	UPROPERTY(EditAnyWhere, BlueprintReadWrite, Category = "Blackboard")
	float Distance = 500.f;

	bool IsDistanceGreaterThanX(FVector Location);
};
