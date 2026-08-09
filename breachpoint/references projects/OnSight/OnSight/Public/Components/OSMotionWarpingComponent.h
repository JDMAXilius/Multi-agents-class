#pragma once

#include "CoreMinimal.h"
#include "MotionWarpingComponent.h"
#include "OSMotionWarpingComponent.generated.h"

/**
 * Thin wrapper over UMotionWarpingComponent for combat use.
 *
 * All warp target injection is handled by GA_OSAttack::InjectWarpTargets,
 * which calls engine APIs (AddOrUpdateWarpTargetFromComponent/Transform) directly.
 * This component exists as the project's concrete MWC type on character blueprints.
 *
 * Architecture: GA (WHEN + WHO + WHERE) -> Notify (TIMING) -> Engine (HOW)
 */
UCLASS(ClassGroup = (OnSight), meta = (BlueprintSpawnableComponent))
class ONSIGHT_API UOSMotionWarpingComponent : public UMotionWarpingComponent
{
	GENERATED_BODY()

public:

	UOSMotionWarpingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
