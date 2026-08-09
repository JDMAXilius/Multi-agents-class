#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSJumpingMovement.generated.h"

/**
 * Short duration: Gameplay.State.Movement.Jumping for anim (ascending jump window).
 * Applied by UGA_OSJump on server after ACharacter::Jump(); stacks refresh on repeat jump.
 */
UCLASS()
class ONSIGHT_API UGE_OSJumpingMovement : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSJumpingMovement(const FObjectInitializer& ObjectInitializer);
};
