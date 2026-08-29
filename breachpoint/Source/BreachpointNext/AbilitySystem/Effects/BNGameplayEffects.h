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

/** ADS's walk speed, sprint's mechanism: MULTIPLY on MoveSpeed, magnitude captured non-snapshot
 *  from the ADSSpeedMultiplier attribute, so removal restores base speed through GE aggregation
 *  and tuning lives in the attribute, never here. */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_ADS : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_ADS();
};

/**
 * R7.4 — the grenade's COST: one grenade, additive, instant. A fixed -1 rather than a SetByCaller
 * on purpose — GAS's own CheckCost calls CanApplyAttributeModifiers on this CDO before the spec
 * exists, so a magnitude that is only filled in at apply time would check nothing. One grenade per
 * throw is also the rule, not a tuning number; the CAPACITY is the number worth tuning, and that
 * lives on MaxGrenades in the init GE.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_GrenadeCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_GrenadeCost();
};

namespace BNSetByCaller
{
	/** UBNGE_FireCooldown's duration key. An FName, not a tag: this GE's magnitude is configured
	 *  in the CDO constructor and native tags are not guaranteed registered while CDOs are built. */
	const FName FireDelay(TEXT("FireDelay"));

	/** UBNGE_GrenadeCooldown's duration key. Same construction-order reason as FireDelay. */
	const FName GrenadeCooldown(TEXT("GrenadeCooldown"));

	/** UBNGE_GrappleCooldown's duration key (BN23). Same construction-order reason. */
	const FName GrappleCooldown(TEXT("GrappleCooldown"));

	/** UBNGE_DashCooldown's duration key. Same construction-order reason as the grapple's. */
	const FName DashCooldown(TEXT("DashCooldown"));

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

/** HEALTH REGEN's numbers (founder, 27 Aug: retreat, heal after a while, re-engage).
 *  Same constants-not-Config rule as BNShield — read in a CDO ctor. Deliberately SLOWER
 *  than the shield: 2.5/period at 0.1s = 25 health per second, a full 100 bar in ~4s —
 *  disengaging buys recovery, it does not erase a fight the way the ~1s shield does. */
namespace BNHealth
{
	constexpr float RegenPeriod = 0.1f;
	constexpr float RegenPerPeriod = 2.5f;
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

/** The Grappleshot's rate limit (BN23) — the grenade cooldown's exact shape. A PREDICTED
 *  cooldown GE on purpose: a server-rejected grapple rolls the cooldown back with the
 *  prediction window, where a hand-tracked float never would (rejection leaves zero state). */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_GrappleCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_GrappleCooldown();
};

/** The dash's cooldown, shaped exactly like the grapple's: duration by SetByCaller so the
 *  ability owns the number, and the Cooldown.Dash tag rides the SPEC rather than the CDO
 *  (a tag added in a CDO constructor is added before the tag tree exists). */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_DashCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_DashCooldown();
};

/**
 * The damage window. Applied by every landed point of damage, it holds State.Combat.RecentDamage
 * for its duration — and that tag is the ONLY thing standing between a hit and the shield starting
 * to come back. The "delay before recharge" is this GE's duration, not a timer anyone runs.
 *
 * DELIBERATELY NOT STACKED, and the reason is descope: UBNGA_ADS cancels on the tag COUNT
 * increasing, which is how it sees hits 2..N while windows overlap. A stack-refresh policy would
 * hold the count at 1 forever under sustained fire and blind descope to every hit after the
 * first — the exact bug the critic caught in the NewOrRemoved listener. The cost is bounded GE
 * churn (window/FireDelay concurrent instances per victim, worst case), recorded and accepted;
 * revisit only with a mechanism that preserves a per-hit signal.
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

/**
 * HEALTH REGEN's engine (founder, 27 Aug) — UBNGE_ShieldRecharge's exact shape with Health
 * swapped in: infinite, periodic, adds Health, knows nothing about when to run.
 * UBNHealthComponent gates it on State.Combat.HealthRegenDelay AND State.Dead — the dead
 * gate is NOT optional: the corpse pawn outlives its lethal hit's window, the ASC persists
 * on the PlayerState, and an ungated regen would raise a dead body's Health above zero,
 * which resets the death latch and resurrects it (the tripwire class the respawn path
 * documents). A distinct class, not a parameterized recharge, because the component
 * removes by class-scoped handle and the two regens gate on different conditions.
 * Both clamps (current and BASE) already cap it at MaxHealth.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_HealthRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_HealthRegen();
};
