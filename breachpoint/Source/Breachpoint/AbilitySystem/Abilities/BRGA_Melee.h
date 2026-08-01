// BREACHPOINT — BP05 step 2. The melee path.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "GameplayEffectTypes.h"

#include "BRGA_Melee.generated.h"

class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;

/**
 * THE MELEE ABILITY — the Golden Triangle's third verb, and the notify-window seam's first
 * consumer.
 *
 * ================================================================================================
 * THE SHAPE, and why every arrow points the way it does (`gas-purity.md` §4, §5)
 * ================================================================================================
 *
 *   press
 *     -> ASC input buffer activates by tag (InputTag.Melee), LocalPredicted
 *     -> the swing montage plays (NOT YET — see THE THREE GAPS below) and its notifies raise
 *        `Event.Melee.WindowBegin` / `Event.Melee.WindowEnd` on the avatar's ASC
 *     -> WindowBegin records WHERE the swing started; WindowEnd closes it
 *     -> CLIENT sweeps once across the window's travel, builds
 *        FGameplayAbilityTargetData_SingleTargetHit
 *     -> ServerSetReplicatedTargetData inside the activation's prediction window
 *     -> SERVER re-derives from ITS OWN transforms and VALIDATES: reach, in-front, line of sight
 *     -> SERVER decides REAR from the victim's forward vector vs the attacker's approach, and
 *        applies GE_Damage (SetByCaller.BaseDamage + Damage.Melee [+ Damage.Rear])
 *     -> EndAbility
 *
 * **CLIENT TARGET DATA IS A CLAIM, NEVER TRUTH.** This is `BRGA_WeaponFire`'s discipline applied
 * to a shorter weapon: the client says "I connected with that fighter"; the server checks that a
 * swing from ITS avatar's eye point, no longer than the reach in `CT_Combat`, in front of ITS
 * view rotation, with nothing solid in between, could have produced that. Everything the client
 * sends is bounded by a server-side number; nothing is taken on trust.
 *
 * **AND THE REAR ARC IS NOT IN THE CLAIM AT ALL.** The client never sends a "this was a backstab"
 * flag and there is no field it could put one in. The server computes it from two transforms it
 * already replicates — the victim's forward vector and the direction from the victim to the
 * attacker — exactly as `BRGA_WeaponFire` decides headshot from the validated hit's bone rather
 * than from the client naming one. Rear-arc spoofing from client view angles is BP05 step 5's
 * REFUTER question, and the answer is that a client view angle is not an input to this decision.
 *
 * ================================================================================================
 * WHAT THIS ABILITY DELIBERATELY DOES NOT DO
 * ================================================================================================
 *
 * - **It does not call the engine damage API.** `TakeDamage`/`ApplyPointDamage` are banned
 *   (`gas-purity.md` law 3, and `guard_laws.py` blocks them at tool-call time). ONE pipeline:
 *   `UBRGE_Damage` + `UBRDamageExecCalc`, the same two classes the AR uses.
 * - **It does not compute a damage number.** It supplies `SetByCaller.BaseDamage` from
 *   `CT_Combat` and the tags `{Damage.Melee}` or `{Damage.Melee, Damage.Rear}`; the exec calc
 *   multiplies each tag's curve. The pair is FLAT (ruling R22) — there is no `Damage.Melee.Rear`
 *   and a grep for one correctly finds nothing. "Rear melee is lethal" is therefore a CSV value
 *   in `Damage.Rear.Multiplier`, not a branch in this file.
 * - **It does not type a single tuning number.** Reach, sweep radius, rear arc and base damage
 *   are `CT_Combat` curves. When they are missing the ability REFUSES TO ACTIVATE and says which
 *   row is absent — see THE THREE GAPS.
 * - **It does not Tick, and it does not trace per frame.** The classic melee implementation
 *   sweeps every frame between the two notifies; that is a Tick with extra steps (law 4). This
 *   one records the swing's origin at WindowBegin and performs ONE swept-sphere query at
 *   WindowEnd, from that origin to the current reach point — a single query that covers the
 *   travel of the window instead of sampling it.
 * - **It spawns no FX.** Law 6: swing and impact are GameplayCues, and no melee cue tag exists
 *   yet (gap 3).
 *
 * ================================================================================================
 * THE THREE GAPS — filed here loudly because none of them is this file's to close
 * ================================================================================================
 *
 *  1. **`Ability.Melee` is not declared.** `Ability.*` is an OPEN family (ruling R23) and the tag
 *     belongs to this packet, but `Core/BRGameplayTags.h` is being written by another agent in
 *     this session and law 5 forbids reaching into it. The constructor names
 *     `BRGameplayTags::Ability_Melee` as the ability's asset tag; **this file does not compile
 *     until that symbol exists** (one `UE_DECLARE_GAMEPLAY_TAG_EXTERN` + one
 *     `UE_DEFINE_GAMEPLAY_TAG_COMMENT`). That is deliberate: a `RequestGameplayTag` by string
 *     would compile and then silently resolve to an invalid tag, which is a melee that no future
 *     ability can cancel and nobody notices.
 *
 *  2. **`CT_Combat` has no melee rows.** Four curves are required and none exists:
 *     `Melee.BaseDamage`, `Melee.RangeMetres`, `Melee.SweepRadiusMetres`, `Melee.RearArcDegrees`.
 *     The table's `Damage.Melee.Multiplier` and `Damage.Rear.Multiplier` rows DO exist, so the
 *     exec calc's half of melee is already data-complete. Until the four land, this ability
 *     refuses in CanActivateAbility and logs the missing name.
 *
 *  3. **No swing montage exists**, so `Event.Melee.WindowBegin`/`WindowEnd` will never fire
 *     today. A watchdog `WaitDelay` stands in, resolves the swing from a structural fallback
 *     window, and LOGS that it ran without animation. It is not a second implementation of the
 *     window — it is the same resolve path with a synthetic close, and it stays afterwards as the
 *     guard against an ability that waits forever for a notify that never comes. `animation.md`
 *     law 4 governs the seam: the notify announces a MOMENT, this file decides the CONSEQUENCE.
 *
 * A FOURTH, SMALLER NOTE: BP05's ticket says the rear-arc check is a "character helper" living in
 * `Source/Breachpoint/Character/`, which is outside this packet's owner_path and is already filed
 * BLOCKED in the ticket's Log. `IsRearHit` below is self-contained and takes no member state, so
 * moving it is a cut and a paste when that folder is granted.
 */
UCLASS()
class BREACHPOINT_API UBRGA_Melee : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Melee(const FObjectInitializer& ObjectInitializer);

	/**
	 * The melee coefficients are a precondition, not a failure. Checked here so a swing with no
	 * reach never opens a prediction window the server would only reject — and so a missing
	 * `CT_Combat` row reads as "melee is refusing, here is the row" rather than as a melee that
	 * mysteriously does nothing.
	 */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	// -- the notify seam ------------------------------------------------------------------------

	/** `Event.Melee.WindowBegin`: the swing's damaging phase starts. Records WHERE, decides nothing. */
	UFUNCTION()
	void OnWindowBegin(FGameplayEventData Payload);

	/** `Event.Melee.WindowEnd`: the damaging phase is over. This is where the one sweep happens. */
	UFUNCTION()
	void OnWindowEnd(FGameplayEventData Payload);

	/** The no-montage stand-in AND the never-leak guard on the attacker's own machine (gap 3). */
	UFUNCTION()
	void OnSwingWatchdogElapsed();

	/** The server's "the client never sent a claim" timeout. Ends the ability; damages nothing. */
	UFUNCTION()
	void OnClaimTimeoutElapsed();

	// -- the swing ------------------------------------------------------------------------------

	/** View origin + direction of the avatar, on whichever side is asking. */
	bool GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	/**
	 * ONE swept-sphere query across the window's travel. Same function on client and server ON
	 * PURPOSE, for the same reason `BRGA_WeaponFire::TraceShot` is: a validation that re-derives
	 * the swing with different code is validating a different swing.
	 */
	FHitResult SweepSwing(const FVector& From, const FVector& To, float SweepRadiusUU) const;

	/** Client half: sweep, package, send inside the prediction window, then resolve locally. */
	void ResolveSwing();

	/** Both halves converge here — the server's validation gate and the client's prediction. */
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag);

	/**
	 * SERVER TRUTH. Returns false and says why when the client's claim is not reachable from the
	 * server's own state. Never trusts a field it can re-derive.
	 */
	bool ValidateClaim(const FHitResult& Claim, float RangeUU, FString& OutReason) const;

	/**
	 * SERVER TRUTH, and never a client claim: is the attacker inside the victim's rear arc?
	 * Computed from the victim's forward vector and the direction from the victim to the attacker,
	 * both flattened to the horizontal plane. Takes no member state — see the owner_path note in
	 * the class comment.
	 */
	bool IsRearHit(const AActor* Victim, float RearArcDegrees) const;

	/**
	 * Applies the one damage GE. Server only, by contract — the exec calc is server truth.
	 * `bRearHit` arrives already decided by IsRearHit(); this function only stamps the tag, so
	 * there is exactly one place in the project that answers "was this a backstab".
	 */
	void ApplyMeleeDamage(const FHitResult& Hit, float BaseDamage, bool bRearHit) const;

	/** Tears down every task this ability created. Called from EndAbility on every path. */
	void ClearTasks();

	// -- state, all of it per-activation and reset in ActivateAbility -----------------------------

	/** WaitGameplayEvent on `Event.Melee.WindowBegin`. Null on the server-for-a-remote-client path. */
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WindowBeginTask;

	/** WaitGameplayEvent on `Event.Melee.WindowEnd`. */
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WindowEndTask;

	/** The swing watchdog (attacker's machine) or the claim timeout (server, remote attacker). */
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> WatchdogTask;

	/** Handle for the server-side target-data delegate, so EndAbility can tear it down. */
	FDelegateHandle TargetDataDelegateHandle;

	/** The avatar's eye point when the window opened — the start of the one sweep. */
	FVector SwingOriginLocation = FVector::ZeroVector;

	/** True between WindowBegin and WindowEnd. Guards a stray WindowEnd that opens nothing. */
	bool bWindowOpen = false;

	/** Set once the swing has been swept, so a duplicate notify cannot produce a second swing. */
	bool bSwingResolved = false;

	/** Set once a claim has been handled, so a duplicate TargetData packet cannot double-damage. */
	bool bClaimHandled = false;
};
