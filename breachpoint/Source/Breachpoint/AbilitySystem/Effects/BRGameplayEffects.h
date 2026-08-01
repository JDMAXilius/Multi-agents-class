// Breachpoint. THE generic GameplayEffect library — seven classes, no assets, no proliferation.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "BRGameplayEffects.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectContextHandle;
struct FGameplayEffectSpecHandle;

/**
 * ===========================================================================================
 * THE GENERIC-GE LIBRARY (ruling R18: C++ classes, never `.uasset`)
 * ===========================================================================================
 *
 * ONE FILE FOR ALL OF THEM, and that is a decision. Each of these classes is a constructor and
 * nothing else; split across seven file pairs the shared authoring pattern becomes invisible and
 * the seventh copy drifts from the first. Read top to bottom, they are a table.
 *
 * NAMING: the design docs call these `GE_Damage`, `GE_Regen`, ... The C++ classes carry the
 * project prefix (law: class prefix `BR`), so `GE_Damage` is `UBRGE_Damage`. That mapping is
 * one-to-one and there are no others.
 *
 * -------------------------------------------------------------------------------------------
 * THE PATTERN, proven on GE_RecentDamage first and then applied mechanically (BP02 step 4's
 * boxed warning). Since UE 5.3 a GameplayEffect's BEHAVIOUR does not live in properties on the
 * effect; it lives in `UGameplayEffectComponent` instanced subobjects the editor is built to
 * author. Authoring one in C++ therefore has exactly four moves:
 *
 *   1. `CreateDefaultSubobject<TComponent>(TEXT("UniqueName"))` in the constructor.
 *      NOT `AddComponent<T>()` — that engine helper calls `NewObject(this, NAME_None)`, and
 *      `NewObject` with the currently-constructing object as Outer is a FATAL check
 *      (`FObjectInitializer::AssertIfInConstructor`). It is meant for runtime-built effects.
 *   2. `GEComponents.Add(Component)` — the array is `protected`, so a subclass may write it.
 *      (`UGameplayEffect::PostInitProperties` will scoop up any default subobject you forget,
 *      behind an `ensureMsgf`; we add explicitly so the ensure never fires.)
 *   3. Configure the component through its own public API — for tags,
 *      `SetAndApplyTargetTagChanges(FInheritedTagContainer)`, which is what writes the effect's
 *      `CachedGrantedTags`. Setting the component's tag container and skipping this call
 *      produces an effect that looks right in a debugger and grants nothing.
 *   4. Set the plain properties (`DurationPolicy`, `Modifiers`, `Executions`, stacking) directly.
 *
 * Epic supports this path deliberately: `UGameplayEffect::PostInitProperties` has a native-class
 * branch ("easy-to-overlook issues when implementing a Gameplay Effect as a native class") that
 * collects the subobjects and runs `IsDataValid` immediately, so a misconfigured native GE
 * reports itself at editor startup rather than at first application.
 *
 * -------------------------------------------------------------------------------------------
 * NO GAMEPLAY NUMBER APPEARS IN THIS FILE. Not one duration, rate, capacity or multiplier. Every
 * magnitude is either SetByCaller (filled by the applier from `CT_Combat`) or an execution
 * (`BRDamageExecCalc`, which reads `CT_Combat` itself). The only literals are structural: which
 * attribute, which policy, which tag. A number typed here would be a law-3 violation AND a
 * second source of truth that silently wins over the CSV.
 */

// ===========================================================================================
// GE_RecentDamage — the pattern-prover. One duration, one granted tag.
// ===========================================================================================

/**
 * Grants `State.Combat.RecentDamage` for a data-driven duration. Applied by
 * `UBRAbilitySystemComponent::ApplyRecentDamageGate()` the instant damage lands.
 *
 * This tag is the ONLY thing standing between a fighter under sustained fire and shield regen:
 * `UBRGE_Regen` blocks on it as an ongoing tag requirement, so "regen starts 2.5 s after the
 * last hit" is a consequence of a tag expiring — no timer code anywhere in the project.
 *
 * STACKING is not an optimization here, it is the rule: AggregateByTarget with a limit of one
 * and refresh-on-application means the SECOND hit extends the same effect instead of stacking a
 * second 2.5 s window. Without it, an AR at 600 RPM leaves 25 live effect instances on a target
 * and the gate's end time becomes an emergent property of a list.
 */
UCLASS(meta = (DisplayName = "GE_RecentDamage"))
class BREACHPOINT_API UBRGE_RecentDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_RecentDamage();

	/** SetByCaller key for the gate duration. An FName, not a tag — see the note in UBRGE_InitStats. */
	static const FName DurationSetByCallerName;
};

// ===========================================================================================
// GE_Damage — the ONE damage pipeline's entry point
// ===========================================================================================

/**
 * The only GameplayEffect in Breachpoint that can reduce a fighter's health, and the only one
 * that may (gas-purity.md law 3). Hitscan, projectile, radial and melee all apply THIS class
 * with their own `SetByCaller.BaseDamage` and their own dynamic `Damage.*` tags.
 *
 * It owns no modifier of its own: `BRDamageExecCalc` computes the final number and writes it to
 * the `IncomingDamage` meta attribute, and `UBRAttributeSet` decides what that means (shields
 * absorb, remainder overflows to health, death is detected). Three responsibilities, three
 * places, one direction of flow.
 */
UCLASS(meta = (DisplayName = "GE_Damage"))
class BREACHPOINT_API UBRGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_Damage();

	/**
	 * Build a damage spec on the SOURCE ASC (correct instigator/attribution) carrying BaseDamage
	 * and the caller's `Damage.*` tags.
	 *
	 * Offered so that every damage source in the project spells the pipeline the same way. A
	 * caller that hand-rolls MakeOutgoingSpec is not doing anything illegal — but it is one
	 * forgotten `AddDynamicAssetTag` away from a headshot that the exec calc cannot see.
	 *
	 * @param DamageTags one TYPE tag plus zero or more MODIFIER tags — the family is FLAT
	 *                   (ruling R22): `{Damage.Melee, Damage.Rear}`, never `Damage.Melee.Rear`.
	 * @return an invalid handle if SourceASC is null; never a silently empty spec.
	 */
	static FGameplayEffectSpecHandle MakeSpec(UAbilitySystemComponent* SourceASC, float BaseDamage, const FGameplayTagContainer& DamageTags, const FGameplayEffectContextHandle& Context);

	/**
	 * Apply a spec built by MakeSpec to one target. SERVER ONLY by contract — the exec calc is
	 * server truth and clients learn the result through attribute replication and cues.
	 *
	 * @return whether the effect was applied (false is logged with the reason).
	 */
	static bool ApplyToTarget(const FGameplayEffectSpecHandle& SpecHandle, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC);
};

// ===========================================================================================
// GE_Regen — shields only, periodic, gated by a tag
// ===========================================================================================

/**
 * Infinite periodic shield regeneration. Applied once per life and left alone; it does not need
 * removing when damage lands, because the ongoing tag requirement inhibits it instead.
 *
 * TWO tags block it, and the second is not decoration:
 *   - `State.Combat.RecentDamage` — the 2.5 s gate.
 *   - `State.Dead` — a corpse does not recharge. Without this a body waiting out a respawn timer
 *     silently comes back with full shields it never earned.
 *
 * PERIOD AND RATE ARE SEPARATE QUESTIONS and both are data. The applier
 * (`UBRAbilitySystemComponent::EnsureShieldRegenActive`) reads points-per-second from
 * `CT_Combat`, reads the tick period from `CT_Combat`, multiplies to get points-per-period, and
 * writes both onto the spec. Change the period in the CSV and the rate per second does not move
 * — which is the property that makes the period a fidelity knob rather than a balance knob.
 *
 * `PeriodicInhibitionPolicy = ResetPeriod`: when the gate lifts, the next tick is a full period
 * away. `NeverReset` would let a fighter bank regen ticks while being shot, and
 * `ExecuteAndResetPeriod` would pay one out instantly at the 2.5 s boundary.
 */
UCLASS(meta = (DisplayName = "GE_Regen"))
class BREACHPOINT_API UBRGE_Regen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_Regen();
};

// ===========================================================================================
// GE_Cooldown — one class, every ability, per-ability tags and durations
// ===========================================================================================

/**
 * The generic cooldown (gas-purity.md law 4: cooldowns are GEs, never hand-tracked floats — GAS
 * predicts them and rolls them back for free when the server rejects an activation).
 *
 * It grants NO tag of its own. `UBRGameplayAbility::ApplyCooldown` injects the ability's own
 * cooldown tag into the spec's `DynamicGrantedTags` and overrides `GetCooldownTags()` to return
 * it, so ONE effect class serves every ability with a per-ability tag and a per-ability duration
 * from data. That is the whole reason there is no `GE_RocketCooldown`.
 */
UCLASS(meta = (DisplayName = "GE_Cooldown"))
class BREACHPOINT_API UBRGE_Cooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_Cooldown();
};

// ===========================================================================================
// GE_InitStats — the spawn/respawn stat write, and the ORDER is load-bearing
// ===========================================================================================

/**
 * Sets MaxHealth, MaxShields, Health and Shields from `CT_Combat`, in THAT ORDER, and the order
 * is the whole design of this class.
 *
 * `UBRAttributeSet::PreAttributeChange` clamps Health to `GetMaxHealth()` **as it currently is**.
 * On a fresh ASC that is zero. Write Health before MaxHealth and a fighter spawns at zero health
 * from a table with correct numbers in it — a bug that looks like a spawn-system bug, a
 * replication bug, or a data bug, and is none of them. Modifiers execute in array order, so the
 * fix is the array order below, asserted by a pinned spec.
 *
 * `Override`, not `Additive`: respawn re-applies this effect, and an additive version would hand
 * a player 200 health on their second life.
 *
 * MAGNITUDES ARE SetByCaller KEYED BY **FName**, not by tag. `SetByCaller.*` is one of the five
 * CLOSED tag families (ruling R23) — it enumerates BaseDamage, RegenRate and CooldownDuration and
 * nothing else — so `SetByCaller.MaxHealth` cannot be invented by this packet. FName-keyed
 * SetByCaller is first-class engine API and needs no tag. Filed as a contract_gap: if the crew
 * would rather these be tags, §3.1 gains four leaves and this class changes two lines.
 */
UCLASS(meta = (DisplayName = "GE_InitStats"))
class BREACHPOINT_API UBRGE_InitStats : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_InitStats();

	/** SetByCaller keys. The applier sets all four or the spec is wrong — there is no partial init. */
	static const FName MaxHealthName;
	static const FName MaxShieldsName;
	static const FName HealthName;
	static const FName ShieldsName;
};

// ===========================================================================================
// GE_Death — the one death mechanism
// ===========================================================================================

/**
 * Infinite, grants `State.Dead`. `UBRGameplayAbility` lists `State.Dead` in ActivationBlockedTags,
 * so applying this ONE effect disables every verb in the game at once (gas-purity.md law 8) —
 * instead of an `if (bIsDead)` in fire, reload, swap, melee, grenade, grapple and sprint, six of
 * which would eventually disagree.
 *
 * Respawn removes it and re-applies `GE_InitStats`. Both halves live on
 * `UBRAbilitySystemComponent` so GameMode calls one function per transition.
 */
UCLASS(meta = (DisplayName = "GE_Death"))
class BREACHPOINT_API UBRGE_Death : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_Death();
};

// ===========================================================================================
// GE_ShieldsBroken — the seventh effect, and why it exists
// ===========================================================================================

/**
 * Infinite, grants `State.Shields.Broken` while Shields are at zero.
 *
 * NOT IN THE TICKET'S LIST OF SIX, and named loudly rather than slipped in. Steps 1-2 filed a
 * contract_gap: `State.Shields.Broken` is declared in `BRGameplayTags` and §3.1, and NOTHING
 * applied it. Purity law 5 forbids applying a State tag by hand, so the tag was either dead or it
 * needed an effect. It needed an effect: this one.
 *
 * Applied and removed by `UBRAbilitySystemComponent::SetShieldsBrokenState`, driven from the
 * attribute set's one reaction point on the transition — not polled, and not applied twice.
 */
UCLASS(meta = (DisplayName = "GE_ShieldsBroken"))
class BREACHPOINT_API UBRGE_ShieldsBroken : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_ShieldsBroken();
};
