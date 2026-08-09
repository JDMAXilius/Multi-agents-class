#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "OSTDMPlayerStart.generated.h"

UCLASS()
class ONSIGHT_API AOSTDMPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	AOSTDMPlayerStart(const FObjectInitializer& ObjectInitializer);

	/** -1 = any team, otherwise explicit team index (0, 1, ...). */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="OnSight|TDM|Spawn")
	int32 TeamIndex = -1;

	/** Spawn score: higher means safer (farther from nearest hostile). */
	UFUNCTION(BlueprintCallable, Category="OnSight|TDM|Spawn")
	float GetSpawnScore(AController* RequestingController) const;
};
