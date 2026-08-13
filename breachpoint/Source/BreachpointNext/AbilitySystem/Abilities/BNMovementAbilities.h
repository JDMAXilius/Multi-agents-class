#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Engine/TimerHandle.h"
#include "BNMovementAbilities.generated.h"

class ACharacter;

UCLASS()
class BREACHPOINTNEXT_API UBNGA_Jump : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnLanded(const FHitResult& Hit);

	UFUNCTION()
	void OnInputRelease(float TimeHeld);

	FActiveGameplayEffectHandle JumpingHandle;
	FActiveGameplayEffectHandle InAirHandle;
};

UCLASS()
class BREACHPOINTNEXT_API UBNGA_Crouch : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * Sprint. Writes NO speed: it applies UBNGE_Sprint, whose MULTIPLY modifier moves the MoveSpeed
 * attribute, and R1's attribute->MaxWalkSpeed delegate on ABNCharacter propagates it.
 *
 * INTENT vs STATE, and the split is the design. The ABILITY is intent: it lives for exactly as
 * long as the key is held. The speed GE and the Sprinting tag are STATE: the gate applies and
 * removes them underneath a still-active ability, so a momentary strafe drops you to base speed
 * and facing forward again restores sprint with no re-press — and the tag always describes what
 * the character is actually doing, which is what the animation reads.
 *
 * LocalPredicted (the base default), and deliberately — unlike UBNGA_Equip. Everything sprint
 * owns is GAS state: a predicted GE on an attribute and a predicted GE carrying a tag, both of
 * which GAS rolls back by prediction key. A rejection therefore leaves ZERO stuck state — the
 * speed GE is discarded, MoveSpeed returns to base, the delegate restores MaxWalkSpeed and the
 * Sprinting tag goes with it. ServerOnly would instead pay ~half an RTT of speed mismatch on
 * EVERY sprint, which is the worse trade.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGA_Sprint : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnInputRelease(float TimeHeld);

	/** Re-evaluates the forward gate without a Tick. Applies/removes sprint STATE; the only
	 *  thing that ends the ability here is a destroyed avatar. */
	void CheckGate();

	/** Applies or removes the tag GE and the speed GE together. Guarded by bSprintApplied rather
	 *  than by a handle, so a failed apply can retry without ever stacking a second GE. */
	void SetSprintActive(bool bActive);

	static bool IsGateHeld(const ACharacter* Character);

	FActiveGameplayEffectHandle SprintingHandle;
	FActiveGameplayEffectHandle SpeedHandle;
	FTimerHandle GateTimer;
	bool bSprintApplied = false;
};

/**
 * Lean. Holds a State.Lean.* tag for as long as the key is held and nothing else — the procedural
 * AimAndLean component is NOT built here. LocalPredicted like the rest, so the authority applies
 * its own copy of the tag GE and Mixed replication carries it to simulated proxies.
 *
 * The sides are mutually exclusive: activating one CANCELS any other lean already running, so at
 * most one State.Lean.* tag is ever held and no consumer has to resolve an ambiguous side.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGA_Lean : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** The side this ability holds. A virtual, not a constructor assignment: native tags are not
	 *  guaranteed registered while CDOs are being built. */
	virtual FGameplayTag GetLeanTag() const;

	/** Newest side wins: cancels any other running lean on this ASC. */
	void CancelOtherLeans(const FGameplayAbilityActorInfo* ActorInfo);

	UFUNCTION()
	void OnInputRelease(float TimeHeld);

	FActiveGameplayEffectHandle LeanHandle;
};

UCLASS()
class BREACHPOINTNEXT_API UBNGA_LeanLeft : public UBNGA_Lean
{
	GENERATED_BODY()

protected:
	virtual FGameplayTag GetLeanTag() const override;
};

UCLASS()
class BREACHPOINTNEXT_API UBNGA_LeanRight : public UBNGA_Lean
{
	GENERATED_BODY()

protected:
	virtual FGameplayTag GetLeanTag() const override;
};
