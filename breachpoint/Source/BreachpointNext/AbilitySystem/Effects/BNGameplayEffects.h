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

	/** UBNGE_GrenadeCooldown's duration key. Same construction-order reason as FireDelay. */
	const FName GrenadeCooldown(TEXT("GrenadeCooldown"));

	/** UBNGE_RecentDamage's duration key — the shield's delay before it starts coming back. */
	const FName RecentDamageWindow(TEXT("RecentDamageWindow"));
}

/** The shield dance's numbers, in one place. Constants rather than Config because they are read
 *  inside a CDO constructor, where config has not been applied yet; UBNHealthComponent carries the
 *  Config-facing delay, which is the one a designer actually retunes. */
namespace BNShield
{
	constexpr float RechargePeriod = 0.1f;
	/** 10/period at 0.1s = 100 shield per second — a full bar in ~1s once it starts. */
	constexpr float RechargePerPeriod = 10.f;
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

/**
 * The grenade's rate limit. It had none: CommitAbility with no cost GE and no cooldown GE returns
 * true and spends nothing, so the only bound was the 0.2s throw delay — about five replicated,
 * 90-damage projectiles per second, indefinitely, all server-side. Duration is SetByCaller so the
 * number lives on the ability rather than in this constructor.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_GrenadeCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_GrenadeCooldown();
};

/**
 * The damage window. Applied by every landed point of damage, it holds State.Combat.RecentDamage
 * for its duration — and that tag is the ONLY thing standing between a hit and the shield starting
 * to come back. The "delay before recharge" is this GE's duration, not a timer anyone runs.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_RecentDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_RecentDamage();
};

/**
 * The shield dance's engine: infinite, periodic, adds Shield. Nothing more — it does not know when
 * to run. UBNHealthComponent applies and removes it as State.Combat.RecentDamage comes and goes.
 *
 * Why not OngoingTagRequirements, which would be self-gating and tidier: those would have to be set
 * in this constructor, and native tags are NOT guaranteed registered while CDOs are built — the
 * same construction-order rule that keeps UBNGE_State's tag on the spec and resolves UBNGA_Death's
 * block tag in a virtual. A requirement built against an invalid tag never fires and fails silently.
 *
 * MaxShield had to exist before this could: an Add modifier with no ceiling climbs forever, and
 * PreAttributeChange's clamp is what stops it at full.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_ShieldRecharge : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_ShieldRecharge();
};
