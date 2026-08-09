// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "ZoransTeamObject.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/TeamData.h"
#include "TeamStateFunctionLibrary.generated.h"

class AZoransTeamStateBase;

UCLASS()
class ZORANSRESISTANCE_API UTeamStateFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", ExpandEnumAsExecs = "Result"))
	static void GetTeamDispositionFromActors(const UObject* WorldContextObject, AActor* FirstActor, AActor* SecondActor, ETeamComparisonResult& Result);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", ExpandEnumAsExecs = "Result"))
	static void GetTeamDispositionByIndex(const UObject* WorldContextObject, int32 FirstIndex, int32 SecondIndex, ETeamComparisonResult& Result);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static UZoransTeamObject* GetLowestPopulationTeam(const UObject* WorldContextObject);
};