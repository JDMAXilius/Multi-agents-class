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
 * LocalPredicted (the base default), and deliberately — unlike UBNGA_Equip. Everything sprint
 * owns is GAS state: a predicted GE on an attribute and a predicted GE carrying a tag, both of
 * which GAS rolls back by prediction key. A rejection therefore leaves ZERO stuck state — the
 * speed GE is discarded, MoveSpeed returns to base, the delegate restores MaxWalkSpeed and the
 * Sprinting tag goes with it; the only residue is a fraction of a second of over-speed motion
 * that the CMC's ordinary position correction resolves. ServerOnly would instead pay ~half an
 * RTT of speed mismatch on EVERY sprint, which is the worse trade.
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

	/** Re-arms the forward gate without a Tick; ends the ability the moment it stops holding. */
	void CheckGate();

	static bool IsGateHeld(const ACharacter* Character);

	FActiveGameplayEffectHandle SprintingHandle;
	FActiveGameplayEffectHandle SpeedHandle;
	FTimerHandle GateTimer;
};

/**
 * Lean. Holds a State.Lean.* tag for as long as the key is held and nothing else — the procedural
 * AimAndLean component is NOT built here. LocalPredicted like the rest, so the authority applies
 * its own copy of the tag GE and Mixed replication carries it to simulated proxies.
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
