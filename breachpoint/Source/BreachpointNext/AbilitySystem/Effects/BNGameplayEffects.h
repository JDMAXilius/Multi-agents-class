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

namespace BNSetByCaller
{
	/** UBNGE_FireCooldown's duration key. An FName, not a tag: this GE's magnitude is configured
	 *  in the CDO constructor and native tags are not guaranteed registered while CDOs are built. */
	const FName FireDelay(TEXT("FireDelay"));
}

/**
 * The fire rate, as a cooldown rather than a timer. Duration is SetByCaller so it is the ROW's
 * FireDelay per weapon — a literal here would be a fire rate living in code. The Cooldown tag
 * rides the spec (UBNGE_State's pattern) for the same construction-order reason.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_FireCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_FireCooldown();
};
