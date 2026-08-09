#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_OSTracking.generated.h"

/** Reference GE_Tracking: infinite, grants Character.State.Tracking, no stacking. */
UCLASS()
class ONSIGHT_API UGE_OSTracking : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSTracking(const FObjectInitializer& ObjectInitializer);
};
