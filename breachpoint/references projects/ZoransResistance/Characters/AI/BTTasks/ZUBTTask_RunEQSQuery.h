// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_RunEQSQuery.h"
#include "ZUBTTask_RunEQSQuery.generated.h"

/**
 * 
 */
class AZoransAIController;
UCLASS()

class ZORANSRESISTANCE_API UZubtTask_RunEQSQuery : public UBTTask_RunEQSQuery
{
	GENERATED_BODY()

public:
	UZubtTask_RunEQSQuery();
	UPROPERTY(Category = EQS, EditAnywhere)
	FEQSParametrizedQueryExecutionRequest EQSRequestT;
};
