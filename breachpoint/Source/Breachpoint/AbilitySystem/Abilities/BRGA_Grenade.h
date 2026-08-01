// BREACHPOINT — BP05 step 1. The grenade: cook, throw, and the server-only projectile spawn.
// (OUR radial damage moved to Weapons/BRExplosion.h when D6 closed — see the class comment.)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"

#include "BRGA_Grenade.generated.h"

class UAbilitySystemComponent;
class UAbilityTask_WaitInputRelease;

/**
 * The numbers a grenade IS. Gathered from data once at activation, never typed.
 *
 * A struct rather than a fistful of locals so that the resolve step has ONE success/failure answer:
 * a grenade that knows its radius but not its fuse is not a partially-configured grenade, it is a
 * refusal. `UBRGA_Grapple` takes the same position about its two missing curve rows and it is the
 * position law 3 forces — a plausible default here would be a balance change nobody can see.
 *
 * ZERO IS THE SENTINEL FOR THE FIVE REQUIRED FIELDS, and that is deliberate: every one of those
 * numbers is meaningless at zero (a zero radius damages nobody, a zero speed drops the grenade at
 * the thrower's feet), so "unset" and "absurd" are the same value and one check covers both.
 *
 * THE TWO BOUNCE FIELDS ARE THE EXCEPTION and their sentinel is NEGATIVE, because zero restitution
 * is a meaningful authored value (a grenade that lands dead where it hits) and must be tellable
 * from an unauthored one. Their rows do not exist in `CT_Combat` yet; `ABRProjectile` explains at
 * length what a negative one means and why that case is a loud warning rather than a refusal.
 */
USTRUCT()
struct FBRGrenadeTuning
{
	GENERATED_BODY()

	/** Maximum hold before the hand releases it anyway. The cook window's length. */
	float CookSeconds = 0.f;

	/** Total seconds from activation to detonation. Time spent cooking is spent OUT OF THIS. */
	float FuseSeconds = 0.f;

	/** Throw speed in metres/second, down the thrower's view ray. Gravity and bounce are the
	 *  projectile's business, not this ability's — so there is no "throw arc" number here. */
	float ThrowSpeedMetresPerSecond = 0.f;

	/** Blast radius in METRES. The overlap query's sphere and the falloff curve's denominator. */
	float BlastRadiusMetres = 0.f;

	/** Damage at the epicentre, BEFORE falloff and before `BRDamageExecCalc`'s multipliers. */
	float BlastCentreDamage = 0.f;

	/**
	 * Bounce restitution, 0..1. **NEGATIVE = the `Grenade.Bounciness` row does not exist**, which is
	 * the state of `CT_Combat` today; `ABRProjectile` then leaves the engine's own value standing and
	 * says so. Unlike the five above, its absence does NOT refuse the throw — the reasoning is at
	 * `ABRProjectile::InitializeProjectile` and it is stated there once rather than twice.
	 */
	float Bounciness = -1.f;

	/** Bounce/slide friction. Negative = the `Grenade.BounceFriction` row does not exist. As above. */
	float BounceFriction = -1.f;
};

/**
 * ================================================================================================
 * THE GRENADE — BP05's first leg
 * ================================================================================================
 *
 * ARCHITECTURE §3.3 states the shape in one sentence: *"Cook -> server-authoritative projectile
 * spawn (client ghost for feel); explosion applies the same damage GE with `Damage.Explosive`."*
 * Three clauses, and each one is a rule this file obeys rather than a feature it implements:
 *
 *   press
 *     -> ASC input buffer activates by tag (InputTag.Grenade), LocalPredicted
 *     -> COOK: a WaitInputRelease task and a timer race each other. Whichever wins throws.
 *     -> the CLIENT raises the throw cue (the "ghost") inside the prediction window
 *     -> the SERVER — and only the server — spawns the projectile
 *     -> EndAbility, on the frame of the throw. The fuse outlives the ability.
 *     -> ... later, on the authority, `ABRProjectile` detonates and calls
 *        `BRExplosion::ApplyExplosionDamage`, which gathers targets ITSELF and applies ONE
 *        GE_Damage per target with {Damage.Explosive}
 *
 * ================================================================================================
 * WHY THERE IS NO `ValidateClaim` IN THIS FILE, unlike `BRGA_WeaponFire`
 * ================================================================================================
 *
 * **The client never tells the server where the grenade went.** No TargetData is sent, so there is
 * nothing to validate. Both sides run the same LocalPredicted ability and each computes the
 * release transform from ITS OWN `GetActorEyesViewPoint` — the server from the replicated control
 * rotation it already trusts for every other purpose. Server truth here is not achieved by
 * checking a claim; it is achieved by never accepting one.
 *
 * The price is that the ghost and the real projectile can leave the hand along slightly different
 * rays under latency. That is the correct trade: the ghost is a GameplayCue, it is cosmetic, and
 * GAS removes it on rollback. A client-authored throw direction would be a permanent cheat surface
 * bought to fix a cosmetic divergence.
 *
 * ================================================================================================
 * THE SERVER-ONLY SPAWN, and why it is a law rather than a preference
 * ================================================================================================
 *
 * `gas-purity` §4 enumerates what may run inside the prediction window — montages, cues, predicted
 * GEs, `WaitTargetData` — and what may not: **spawning authoritative actors**. It names this exact
 * case ("rocket projectile spawns server-side; the client shows a cue ghost"). A predictively
 * spawned actor has no rollback path: GAS does not know it exists, so a rejected activation leaves
 * a live grenade on one machine and nothing on the other. This is the same class of ruling as the
 * Grappleshot's one-body decision — the netcode decides the shape, and the shape then costs
 * nothing to keep.
 *
 * ================================================================================================
 * D6 IS CLOSED — the projectile exists, and the blast rule moved out with it
 * ================================================================================================
 *
 * This file spent a packet blocked on `docs/DECISIONS-OWED.md` **D6** ("which §3 folder owns the
 * projectile?"), with `RequestProjectileSpawn` as a named seam that logged what the projectile
 * owed instead of spawning one. D6 took option (a): **`Weapons/BRProjectile`**, §3.5's fourth unit,
 * sim-builder's. The seam is now a call and its old specification is that class's comment.
 *
 * TWO CONSEQUENCES LAND HERE, and the second is the one worth reading:
 *
 * 1. `RequestProjectileSpawn` builds an `FBRProjectileSpawnParams` and spawns. Everything the
 *    projectile needs at detonation — radius, centre damage, the falloff curve's NAME, the explode
 *    cue tag — is resolved on THIS side and travels WITH the spawn. It is never re-read at
 *    detonation: one grenade, one set of numbers, agreed at the moment it left the hand.
 *
 * 2. **`ApplyExplosionDamage` IS GONE FROM THIS FILE.** It now lives at
 *    `Weapons/BRExplosion.h`, as a free function both the grenade's projectile and BP09's rocket
 *    call. This file had already filed the reason against itself: a projectile in `Weapons/`
 *    calling `UBRGA_Grenade::ApplyExplosionDamage` would REVERSE the dependency arrow. Four files
 *    in `AbilitySystem/Abilities/` include `Weapons/` — `BRGA_WeaponFire`, `BRGA_WeaponUtility`,
 *    `BRGA_Grapple` and this one — and nothing in `Weapons/` includes `Abilities/`. Moving the
 *    blast preserved that, and `BRExplosion.h`'s header carries the rest of the argument along with
 *    every decision the function made (LOS through world geometry only, distance to actor centre,
 *    the thrower is a target, friendly fire deliberately undecided).
 */
UCLASS()
class BREACHPOINT_API UBRGA_Grenade : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Grenade(const FObjectInitializer& ObjectInitializer);

	/**
	 * The stage gate lives HERE, not in ActivateAbility, and the reason is bots.
	 *
	 * `ABRBotController` builds `FBRBotFacts::bCanGrenade` / `bCanGrapple` by CALLING
	 * `CanActivateAbility` on the granted spec. With the gate one level down in
	 * `ActivateAbility`, a bot below the stage still believes the verb is available, presses it,
	 * and is refused at activation — a bot reasoning from a fact that is false. Declaring this
	 * override makes the bot's picture and the gate's answer the same answer.
	 *
	 * It also refuses BEFORE `PreActivate`, so no prediction key is spent and no
	 * `ServerTryActivateAbility` crosses the wire for an ability the stage has switched off.
	 */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// OUR RADIAL DAMAGE MOVED. `UBRGA_Grenade::ApplyExplosionDamage` is now
	// `BRExplosion::ApplyExplosionDamage` in `Weapons/BRExplosion.h` — same body, same decisions,
	// on the correct side of the dependency arrow. It was always static precisely because the
	// detonating object is the projectile and not an ability instance; the move is that fact
	// finishing its sentence. Nothing calls it from this file any more: the projectile does, with
	// the numbers this ability resolved and handed it at the spawn. It is still reachable from a
	// headless spec, which is what BP05 step 4 needs for "radial falloff exact cases".

protected:
	/**
	 * THE COST, overridden rather than inherited — and the override is the whole point.
	 *
	 * `CostGameplayEffectClass` alone would be a trap: the engine's default CheckCost/ApplyCost
	 * build their own spec and set no SetByCaller, so `UBRGE_GrenadeCost`'s magnitude evaluates
	 * to 0 — a cost that is wired, looks wired, and costs nothing. These supply the number.
	 */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
	/** One builder for both cost paths, so CheckCost and ApplyCost can never disagree about the
	 *  magnitude. Invalid handle = refuse; never a default cost. */
	FGameplayEffectSpecHandle MakeCostSpec() const;


	/** The cook's release half. UFUNCTION because `OnRelease` is a dynamic delegate. */
	UFUNCTION()
	void HandleCookReleased(float TimeHeld);

	/** The cook's timer half — the hand lets go by itself at `CookSeconds`. */
	void HandleCookExpired();

	/**
	 * Gather the numbers from their lawful homes, or say which one is missing.
	 * @return false with a human-readable reason; never a partially-filled tuning. The two bounce
	 *         fields are the one exception and cannot cause a false — see `FBRGrenadeTuning`.
	 */
	bool ResolveTuning(FBRGrenadeTuning& OutTuning, FString& OutReason) const;

	/**
	 * The one place the grenade leaves the hand. Both cook paths converge here and it is guarded
	 * against running twice, because release and timeout CAN land on the same frame.
	 *
	 * @param SecondsCooked  time spent holding. The remaining fuse is `FuseSeconds - SecondsCooked`,
	 *                       which is the entire point of cooking.
	 */
	void ThrowGrenade(float SecondsCooked);

	/** THE SEAM, and it is now a real spawn. Authority only. `Weapons/BRProjectile.h` is the far side. */
	void RequestProjectileSpawn(const FTransform& ReleaseTransform, const FVector& LaunchVelocity, float RemainingFuseSeconds) const;

	/** Tear down the cook. Idempotent, and called from every exit including cancellation. */
	void ClearCook();

	/** View origin + direction on whichever side is asking. Same seam the fire path uses. */
	bool GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	/**
	 * Tags this packet NEEDS and may not DECLARE, resolved by name at runtime.
	 *
	 * `Ability.Grenade` and the two `GameplayCue.Grenade.*` leaves belong in
	 * `Core/BRGameplayTags.h`, which R23 makes an OPEN family a packet may append to — but only
	 * under an exact-file grant, and this packet does not hold one (that file is being edited by
	 * another owner right now). Requesting by string with `ErrorIfNotFound=false` returns an
	 * INVALID tag when the leaf is not declared, which every caller below treats as a loud refusal.
	 *
	 * That is the honest bridge, and it is temporary by construction: the moment the leaf is
	 * declared, each of these becomes a one-line `return BRGameplayTags::Ability_Grenade;` and the
	 * string disappears. A string literal that silently resolved to a default would be the
	 * `DT_Weapons.FireCueTag` defect again — a dangling cross-artifact reference that passes every
	 * gate for three days.
	 */
	static FGameplayTag RequestOwedTag(const TCHAR* TagString);

	/** The release watcher. Held so EndAbility can end it on every path, including cancel. */
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> CookReleaseTask;

	/** The cook's ceiling. A TIMER, not a Tick (law 4). */
	FTimerHandle CookTimerHandle;

	/** Resolved once at activation so the cook and the throw cannot read two different grenades. */
	FBRGrenadeTuning Tuning;

	/** Set the instant the grenade leaves the hand, so release + timeout on one frame throws once. */
	bool bThrowResolved = false;
};
