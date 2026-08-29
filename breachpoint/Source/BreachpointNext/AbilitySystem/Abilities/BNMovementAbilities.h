#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Engine/TimerHandle.h"
#include "Templates/SubclassOf.h"

#include "BNMovementAbilities.generated.h"

class ACharacter;
class UAnimMontage;
class UCameraShakeBase;

UCLASS()
class BREACHPOINTNEXT_API UBNGA_Jump : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnLanded(const FHitResult& Hit);

	/** DEBT A1: the avatar can be destroyed in mid-air, and then it never lands. */
	UFUNCTION()
	void OnAvatarDestroyed(AActor* DestroyedActor);

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

/**
 * THE DASH (founder, 29 Aug) — a thrust, not a sprint.
 *
 * DIRECTION IS THE WHOLE DESIGN. It launches along the player's MOVEMENT INPUT, not the
 * camera: a forward-only dash is just a faster sprint, while dashing sideways and backwards
 * is what makes this a dodge — the thing you press when a burst is already coming at you.
 * With no movement held it falls back to camera-forward, so the key is never dead.
 *
 * LaunchCharacter, deliberately, and NOT a custom movement mode like the grapple's. The
 * grapple needed one because a PULL is sustained and has to be re-simulated every frame of
 * the move; a dash is an IMPULSE, and PendingLaunchVelocity is already consumed inside
 * PerformMovement and replayed with the saved move. A second compressed-flag path would be
 * real cost for a value the engine already carries correctly.
 *
 * XY override on, Z override OFF. Overriding horizontal makes the dash a constant — the same
 * distance whether you were sprinting or standing, which is what a dodge has to be to be
 * trusted. Leaving Z alone means it never cancels a fall and never grants free height, so it
 * cannot become a jump.
 */
UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNGA_Dash : public UBNGameplayAbility
{
	GENERATED_BODY()

public:
	UBNGA_Dash();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	/** Launch speed, and it is only half the story — see SetFrictionSuppressed. MEASURED
	 *  against the character's real values (ground friction 8 x factor 2, braking 2048):
	 *  with friction live, 1200 carries 39uu and even 3200 carries only 124uu, because the
	 *  burst is dead in about 0.15s. Distance is set by the WINDOW, not the speed, so the
	 *  ability suppresses friction for DashDuration and the dash covers speed x duration. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash")
	float DashSpeedUU = 2000.f;

	/** How long State.Movement.Dashing is held. Only the tag's lifetime — the velocity is the
	 *  launch's and decays on its own — so this is "how long the dash READS as happening" for
	 *  anim, cues, and anything that gates on it. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash")
	float DashDuration = 0.25f;

	/** Seconds between dashes. Long enough that a dodge is a decision rather than a movement
	 *  tax you pay every fight. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash")
	float CooldownDuration = 3.f;

	/** Air dashing. TRUE: a thrust that stops working the moment you leave the ground is a
	 *  thrust you cannot use to escape the thing that knocked you off it. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash")
	bool bAllowInAir = true;

	// -- POLISH ---------------------------------------------------------------------------
	// FOUR montages, not one, because the template ships all four and a dash that always
	// plays the forward animation while the body slides sideways is worse than no animation:
	// it actively tells the player the game is lying about the direction.

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash|Polish")
	TSoftObjectPtr<UAnimMontage> MontageForward;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash|Polish")
	TSoftObjectPtr<UAnimMontage> MontageBackward;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash|Polish")
	TSoftObjectPtr<UAnimMontage> MontageLeft;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash|Polish")
	TSoftObjectPtr<UAnimMontage> MontageRight;

	/** Camera kick. TSoftClassPtr, NOT TSubclassOf — a shake is a CLASS the manager
	 *  instantiates, and TSubclassOf does not resolve from a config path: it reads back
	 *  None and the shake silently never plays. UBNGameplayCue_Base::Shake carries the same
	 *  type with the same warning, and this property was written the wrong way first and
	 *  caught only because the live CDO was read back. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash|Polish")
	TSoftClassPtr<UCameraShakeBase> CameraShake;

	/** Rumble intensity for the launch. Dynamic force feedback, so it needs no asset and
	 *  works the moment the key is pressed. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash|Polish")
	float HapticIntensity = 0.55f;

	/** Seconds of rumble. Matched to the launch, not to the cooldown — a dash should feel
	 *  like a shove, and a shove is over quickly. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Dash|Polish")
	float HapticDuration = 0.16f;

	/** Picks the montage whose direction matches the dash, in the pawn's own frame. */
	UAnimMontage* SelectDirectionalMontage(const FVector& WorldDirection, const AActor* Avatar, float& OutRollSign) const;

	/** Closes the dash window. Its own function because a timer delegate cannot bind to
	 *  EndAbility's signature, and because the cancel path must be able to clear it. */
	void EndDashWindow();

	/** Suppresses ground friction and braking for the dash window, and puts them back.
	 *  Called on activate and unconditionally on end — including a cancel, because a player
	 *  left frictionless slides forever. */
	void SetFrictionSuppressed(bool bSuppressed);

private:
	FActiveGameplayEffectHandle DashingHandle;
	FTimerHandle DashTimer;
	mutable FGameplayTagContainer CooldownTags;

	/** The character's own values, cached the instant before they are zeroed. -1 means
	 *  "nothing cached", so a double-restore can never write a bogus friction. */
	float CachedGroundFriction = -1.f;
	float CachedBrakingDeceleration = -1.f;
};
