#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSGrabClaim.generated.h"

/**
 * Infinite-duration claim effect applied to the grab victim immediately after box trace succeeds.
 * Grants Gameplay.State.IsBeingGrabbed to prevent a second attacker from grabbing the same victim
 * during the window between trace success and GA_OSGrabReaction activation.
 * Removed via stored FActiveGameplayEffectHandle in GA_OSGrab::EndAbility.
 */
UCLASS()
class ONSIGHT_API UGE_OSGrabClaim : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSGrabClaim(const FObjectInitializer& ObjectInitializer);
};
