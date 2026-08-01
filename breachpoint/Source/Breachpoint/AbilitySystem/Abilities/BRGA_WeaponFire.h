// BREACHPOINT — BP03 step 2. The fire path.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "BRGA_WeaponFire.generated.h"

class UBRWeaponInstance;
struct FBRWeaponRow;

/**
 * THE HITSCAN FIRE ABILITY — the one place a shot becomes damage.
 *
 * ================================================================================================
 * THE SHAPE, and why every arrow points the way it does (`gas-purity.md` §5)
 * ================================================================================================
 *
 *   press
 *     -> ASC input buffer activates by tag (InputTag.Fire), LocalPredicted
 *     -> CLIENT traces from the camera, builds FGameplayAbilityTargetData_SingleTargetHit
 *     -> ServerSetReplicatedTargetData inside the activation's prediction window
 *     -> SERVER re-derives the trace and VALIDATES: ammo, rate, range, cone
 *     -> SERVER consumes ammo and applies GE_Damage (SetByCaller.BaseDamage + Damage.Kinetic
 *        [+ Damage.Headshot])
 *     -> fire cue plays predictively on the shooter, replicates to everyone else
 *     -> EndAbility, same frame
 *
 * **CLIENT TARGET DATA IS A CLAIM, NEVER TRUTH.** The client says "I hit that head at 30 m".
 * The server checks that a shot from ITS muzzle, down a direction within `Spread_deg` of ITS
 * view, no longer than `Range_m`, could have produced that. Everything the client sends is
 * bounded by a server-side number from `DT_Weapons`; nothing is taken on trust. That is law 1,
 * and it is also the entire anti-cheat surface of a shooter.
 *
 * ================================================================================================
 * WHAT THIS ABILITY DELIBERATELY DOES NOT DO
 * ================================================================================================
 *
 * - **It does not call the engine damage API.** `TakeDamage`/`ApplyPointDamage` are banned
 *   (law 2, and the pre-tool hook blocks them). ONE pipeline: `GE_Damage` + `BRDamageExecCalc`.
 * - **It does not compute a damage number.** It supplies `SetByCaller.BaseDamage` from the row
 *   and the `Damage.*` tags; `BRDamageExecCalc` reads `CT_Combat` and decides what they mean.
 *   Headshot is a TAG here, a MULTIPLIER there.
 * - **It does not spawn anything.** Hitscan has no projectile. The rocket is
 *   `DamageDelivery == Projectile` and is a different ability (BP09); this one refuses that row
 *   outright rather than silently hitscanning a rocket.
 * - **It does not roll random spread.** `Spread_deg` is a server ACCEPTANCE CONE, not a
 *   per-shot deviation — see `FBRWeaponRow::Spread_deg`. Unseeded randomness is a banned API,
 *   and on a LocalPredicted ability it would also make client and server disagree about every
 *   shot. The law and the netcode want the same thing here.
 * - **It does not Tick.** Automatic fire is the input buffer re-activating against a cooldown
 *   derived from RPM (law 4).
 *
 * Ammo is the NAMED GAS EXCEPTION (`gas-purity.md` §8 ledger): it lives on the replicated
 * `UBRWeaponInstance`, is server-mutated, and is checked here through `CanActivateAbility`
 * rather than being an attribute with a cost GE.
 */
UCLASS()
class BREACHPOINT_API UBRGA_WeaponFire : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_WeaponFire(const FObjectInitializer& ObjectInitializer);

	/**
	 * Ammo and a hitscan row are preconditions, not failures. Checked here so a dry weapon
	 * never opens a prediction window that the server would only reject — a rejected
	 * activation costs a mispredicted cue and a correction.
	 */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Rate of fire IS the cooldown: 60 / RPM seconds. Read from the row, never typed. */
	virtual float GetCooldownDurationSeconds() const override;

private:
	/** The equipped weapon, or null. Route every access through here — no cached raw pointers. */
	UBRWeaponInstance* GetActiveWeapon() const;

	/** The active weapon's row, or null if unequipped / the table is not imported yet. */
	const FBRWeaponRow* GetWeaponRow() const;

	/** View origin + direction of the avatar, on whichever side is asking. */
	bool GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	/** The trace both sides run. Same function on client and server ON PURPOSE — a validation
	 *  that re-derives the shot with different code is validating a different shot. */
	FHitResult TraceShot(const FBRWeaponRow& Row, const FVector& From, const FVector& Direction) const;

	/** Client half: trace, package, send inside the prediction window, then resolve locally. */
	void FireLocally();

	/** Both halves converge here. On the server this is the validation gate; on the client it
	 *  is the prediction. `bIsServerAuthoritative` says which. */
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag);

	/**
	 * SERVER TRUTH. Returns false and says why when the client's claim is not reachable from
	 * the server's own state. Never trusts a field it can re-derive.
	 */
	bool ValidateClaim(const FHitResult& Claim, const FBRWeaponRow& Row, FString& OutReason) const;

	/** Applies the one damage GE. Server only, by contract — the exec calc is server truth. */
	void ApplyDamage(const FHitResult& Hit, const FBRWeaponRow& Row) const;

	/** Handle for the server-side wait task, so EndAbility can tear it down on every path. */
	FDelegateHandle TargetDataDelegateHandle;

	/** Set once the shot has been resolved, so a duplicate TargetData packet cannot double-fire. */
	bool bShotResolved = false;
};
