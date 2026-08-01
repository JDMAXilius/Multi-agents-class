// BREACHPOINT — BP06. The Grappleshot: the netcode packet.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "BRGA_Grapple.generated.h"

class ABRWeaponPickup;
class UBRCharacterMovementComponent;

/**
 * What the aimed trace found, and therefore which of the three grapple behaviours runs.
 *
 * Classification happens on BOTH sides and must agree, so it is derived ONLY from things both
 * sides can see: the hit actor's type. Nothing here reads client-supplied state.
 */
UENUM()
enum class EBRGrappleMode : uint8
{
	/** Nothing valid was hit. Not a mode — the ability refuses before committing. */
	None,

	/** Static geometry: the SHOOTER is pulled to the point. The common case. */
	SelfPull,

	/** An `ABRWeaponPickup`: the weapon flies to the hand. The shooter does not move. */
	WeaponAttract,

	/** Another pawn: the SHOOTER is pulled to them. See the one-body ruling in the .cpp. */
	PawnReel
};

/**
 * ================================================================================================
 * THE GRAPPLESHOT — and why this is the ticket with a mandatory critic gate
 * ================================================================================================
 *
 * Every other ability in this project either applies an effect or moves a number. This one moves
 * a PLAYER, predictively, and a predicted movement bug is invisible in single-process PIE by
 * construction — in one process the "client" reads server state directly and everything looks
 * perfect. That is why BP06's Done-when demands rung 4 under `-PktLag=120 -PktLoss=5` and why
 * `cmc-prediction` §6 says a claim from the editor is not a claim.
 *
 * ================================================================================================
 * THE DIVISION OF LABOUR — the one rule that decides where every line goes
 * ================================================================================================
 *
 *   BRGA_Grapple (this file)         decides WHETHER: trace, classify, server-validate the
 *                                    target (range, LOS, rate), commit the cooldown
 *   UBRCharacterMovementComponent    executes MOTION: applies the root-motion source, owns the
 *                                    detach rules (arrival radius, jump-cancel, timeout)
 *
 * **This file contains no position write.** When unsure where a line belongs, ask which of those
 * two sentences it is. `gas-purity.md`'s movement exception says it in four words: GAS decides,
 * CMC moves.
 *
 * ================================================================================================
 * "REJECTION LEAVES ZERO STATE" — three residues, three different mechanisms
 * ================================================================================================
 *
 * BP06's acceptance criterion. Each residue is prevented by a different lawful choice, and they
 * are easy to get individually right and collectively wrong (`cmc-prediction` §5):
 *
 *   POSITION  the RMS was never applied server-side, and the client's replay drops it on
 *             correction — because the pull rides the saved-move pipeline instead of being a
 *             manual position write.
 *   COOLDOWN  the cooldown is a **predicted GE** (`GE_Cooldown` + this ability's own tag), so
 *             GAS rolls it back automatically. A hand-tracked float would not.
 *   CUE       rope and impact are GameplayCues raised inside the prediction window, which GAS
 *             removes on rollback. A particle spawned from ability body code stays on screen
 *             forever.
 *
 * **This is the strongest argument for purity anywhere in the project**: three rollback
 * behaviours arrive free from three lawful choices. Break any one and you hand-build its
 * correction path, badly, under time pressure.
 */
UCLASS()
class BREACHPOINT_API UBRGA_Grapple : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Grapple(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 20 s (BP06 step 2), read from `CT_Combat` — never typed here. */
	virtual float GetCooldownDurationSeconds() const override;

private:
	/** The CMC, already typed. Null before actor info is set. */
	UBRCharacterMovementComponent* GetBRMovement() const;

	/** The aimed trace. Run identically on both sides — a validation that re-derives the shot
	 *  with different code is validating a different shot. */
	bool TraceForTarget(FHitResult& OutHit) const;

	/** What did we hit? Derived only from the actor's type, so both sides agree. */
	EBRGrappleMode ClassifyHit(const FHitResult& Hit) const;

	/** SERVER TRUTH. Range, line of sight, and mode agreement. Says why when it says no. */
	bool ValidateTarget(const FHitResult& Claim, EBRGrappleMode ClaimedMode, FString& OutReason) const;

	/** Hands the pull to the CMC. The only thing this file does about motion is ask for it. */
	void BeginPull(const FHitResult& Hit, EBRGrappleMode Mode);

	/** Max grapple range in metres, from `CT_Combat`. */
	float GetMaxRangeMetres() const;

	/** Set once the grapple has been resolved, so a duplicate TargetData packet cannot re-fire. */
	bool bResolved = false;

	/** Which mode actually ran, so EndAbility can tear down exactly what it started. */
	EBRGrappleMode ActiveMode = EBRGrappleMode::None;
};
