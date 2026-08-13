#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "BNGameplayEffects.generated.h"

UCLASS()
class BREACHPOINTNEXT_API UBNGE_InitAttributes : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_InitAttributes();
};

UCLASS()
class BREACHPOINTNEXT_API UBNGE_State : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_State();
};

/**
 * Sprint's speed, and the ONLY thing that changes it. A MULTIPLY modifier on MoveSpeed, magnitude
 * captured from the SprintSpeedMultiplier attribute — so removal restores the base through GE
 * aggregation with no "previous speed" stored anywhere, and MaxWalkSpeed is never written by hand.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_Sprint : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_Sprint();
};
