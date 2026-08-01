// BREACHPOINT — BP05 step 1. The grenade: cook, throw, and OUR radial damage.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"

#include "BRGA_Grenade.generated.h"

class UAbilitySystemComponent;
class UAbilityTask_WaitInputRelease;

/**
 * The six numbers a grenade IS. Gathered from data once at activation, never typed.
 *
 * A struct rather than six locals so that the resolve step has ONE success/failure answer: a
 * grenade that knows its radius but not its fuse is not a partially-configured grenade, it is a
 * refusal. `UBRGA_Grapple` takes the same position about its two missing curve rows and it is the
 * position law 3 forces — a plausible default here would be a balance change nobody can see.
 *
 * ZERO IS THE SENTINEL FOR EVERY FIELD, and that is deliberate: every one of these numbers is
 * meaningless at zero (a zero radius damages nobody, a zero speed drops the grenade at the
 * thrower's feet), so "unset" and "absurd" are the same value and one check covers both.
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
};

/**
 * ================================================================================================
 * THE GRENADE — BP05's first leg, and the one ability whose main deliverable has no home
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
 *     -> ... later, on the authority, the projectile detonates and calls ApplyExplosionDamage,
 *        which gathers targets ITSELF and applies ONE GE_Damage per target with {Damage.Explosive}
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
 * *** THE BLOCKER: THERE IS NO PROJECTILE CLASS, AND §3 HAS NO HOME FOR ONE ***
 * ================================================================================================
 *
 * `docs/DECISIONS-OWED.md` **D6** (RULING, unanswered): ARCHITECTURE §3.5 `Weapons/` enumerates
 * three units and none of them is a projectile; no other §3 folder claims one. BP05's own Log
 * escalated it and said why it cannot be fixed at claim time: *"you cannot grant a path to a file
 * the architecture never named."*
 *
 * **This packet therefore does not create one.** `RequestProjectileSpawn` is a single, named seam
 * that runs on the authority and today logs exactly what the projectile owes. The alternative —
 * inventing `ABRGrenadeProjectile` in some folder and hoping — is the improvisation law 5 exists
 * to prevent, and it would make D6 harder to answer rather than easier, because the ruling would
 * arrive to find a fait accompli in the wrong place.
 *
 * The seam's contract is written out in full at `RequestProjectileSpawn`. Everything on the far
 * side of it is blocked; everything on this side — cook, throw, and the whole radial damage rule —
 * is written, and `ApplyExplosionDamage` is deliberately callable without an ability instance so
 * the projectile can call it the day it exists and a spec can call it today.
 */
UCLASS()
class BREACHPOINT_API UBRGA_Grenade : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Grenade(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * ============================================================================================
	 * OUR RADIAL DAMAGE — the ticket's actual deliverable, and the reason `ApplyRadialDamage` is
	 * not merely banned but unnecessary.
	 * ============================================================================================
	 *
	 * `gas-purity.md` law 3: *"Radial = our own overlap query -> per-target GE application (grenade
	 * and rocket share it)."* Radial damage in this project is four steps, all of them here:
	 *
	 *   1. overlap a sphere for pawns, on the AUTHORITY
	 *   2. drop anything the blast cannot see (an overlap alone damages through walls)
	 *   3. scale the centre damage by the falloff curve at the normalised distance
	 *   4. apply ONE `UBRGE_Damage` per surviving target, tagged `{Damage.Explosive}` — FLAT (R22)
	 *
	 * **STATIC, AND THAT IS THE DESIGN.** By the time a grenade detonates, this ability has ended
	 * (the throw is the end; the fuse is seconds later, and an ability instance that outlives its
	 * own verb would survive a death and respawn holding blast state — `BRGameplayAbility`'s "holds
	 * no per-life state" contract forbids exactly that). The detonating actor is the projectile, and
	 * it must be able to call the rule without owning an ability. Static also makes the rule
	 * reachable from a headless spec, which is what BP05 step 4 needs for "radial falloff exact
	 * cases".
	 *
	 * **KNOWN ARROW PROBLEM, filed not hidden.** If D6 rules the projectile into `Weapons/` (its
	 * recommendation), that folder calling into `AbilitySystem/Abilities/` reverses the arrow
	 * `BRGA_WeaponFire` establishes (ability -> weapon). The honest resolution when the projectile
	 * lands is to move this function onto the projectile, or into a shared explosion helper that
	 * both the grenade and the rocket (BP09) call. It lives here today because
	 * `AbilitySystem/Abilities/` is the only folder this packet may write, and the radial-damage
	 * deliverable has to exist somewhere real rather than be promised.
	 *
	 * SERVER ONLY. Refuses loudly off the authority rather than half-applying a blast that the
	 * clients would each compute differently.
	 *
	 * @param InstigatorASC       the THROWER's ASC — the source of the spec, so kill credit,
	 *                            instigator tags and the effect context all attribute correctly.
	 *                            A projectile that outlives its thrower passes null and is refused.
	 * @param InstigatorActor     the thrower's avatar, for the effect context's instigator pair.
	 * @param Epicentre           world-space detonation point.
	 * @param BlastRadiusMetres   from data; see FBRGrenadeTuning. Zero or negative is refused.
	 * @param BlastCentreDamage   from data; damage at distance 0, before falloff and before the
	 *                            exec calc's `Damage.Explosive.Multiplier`.
	 * @return the number of targets the blast actually damaged. Zero is a legitimate answer (a
	 *         grenade in an empty room) — the log distinguishes it from a refusal.
	 */
	static int32 ApplyExplosionDamage(UAbilitySystemComponent* InstigatorASC, AActor* InstigatorActor, const FVector& Epicentre, float BlastRadiusMetres, float BlastCentreDamage);

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
	 * Gather all six numbers from their lawful homes, or say which one is missing.
	 * @return false with a human-readable reason; never a partially-filled tuning.
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

	/** THE SEAM. Authority only. Blocked on D6 — read the contract at the definition. */
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
