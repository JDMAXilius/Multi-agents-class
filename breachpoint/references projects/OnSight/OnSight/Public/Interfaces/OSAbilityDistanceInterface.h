#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OSAbilityDistanceInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class UOSAbilityDistanceInterface : public UInterface
{
	GENERATED_BODY()
};

/** Provides melee optimal distance for attack abilities.
 *  Implemented by GA_OSAttack — returns OptimalMeleeDistance used as standoff offset in InjectWarpTargets. */
class ONSIGHT_API IOSAbilityDistanceInterface
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "OnSight|Motion Warping")
	virtual bool HasMeleeOptimalDistance() const { return false; }

	UFUNCTION(BlueprintCallable, Category = "OnSight|Motion Warping")
	virtual float GetMeleeOptimalDistance() const { return 0.f; }
};
