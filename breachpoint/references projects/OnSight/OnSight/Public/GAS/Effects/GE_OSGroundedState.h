#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSGroundedState.generated.h"

/**
 * Infinite: grants Gameplay.State.Movement.Grounded while walking / nav walking.
 * Applied/removed with GE_OSInAirState by AOSCharacter (server-only), for anim and gating.
 */
UCLASS()
class ONSIGHT_API UGE_OSGroundedState : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSGroundedState(const FObjectInitializer& ObjectInitializer);
};
