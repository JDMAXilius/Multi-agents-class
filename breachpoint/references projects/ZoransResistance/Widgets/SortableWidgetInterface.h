// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SortableWidgetInterface.generated.h"


UINTERFACE(Blueprintable)
class ZORANSRESISTANCE_API USortableWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

class ISortableWidgetInterface
{    
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	float GetSortValue();


};