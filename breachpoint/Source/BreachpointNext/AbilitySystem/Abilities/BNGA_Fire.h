#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Engine/TimerHandle.h"
#include "BNGA_Fire.generated.h"

struct FGameplayAbilityTargetDataHandle;

/**
 * Fire. LocalPredicted, input tag Input.Weapon.Fire, granted by the WEAPON's ability set — unlike
 * swap, which Wave 2 moved to the PlayerState because it revoked itself by running. Fire does not
 * revoke itself, so weapon-granting is correct and is what the row's AbilitySet column is for.
 *
 * The ability produces a server-validated hit and hands it to BNDamage — the one door — from
 * OnTargetDataReceived, against the SERVER's hit result and nothing client-supplied. No engine
 * damage API is ever called, so there is no second damage path to unbuild.
 *
 * Who does what: the LOCALLY-CONTROLLED machine owns cadence — it activates, runs the repeat, and
 * decides when fire ends. The AUTHORITY owns the truth — it pays ammo, enforces the rate, and is
 * the only judge of a hit. The server deliberately runs no fire timer of its own: two timers on
 * two clocks drift, and every shot already announces itself as one TargetData set.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGA_Fire : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// Ammo as a GAS COST, not a check standing beside the commit path. CheckCost/ApplyCost are the
	// sanctioned extension point for a cost that is not an attribute (Lyra does the same for item
	// stacks), so a shot that cannot pay is refused by CommitAbility itself.
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// Fire rate as a cooldown GE whose duration is the row's FireDelay via SetByCaller.
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	UFUNCTION()
	void OnInputRelease(float TimeHeld);

	/** Server side of the shot: the client's claim arrives here and is judged, never trusted. */
	UFUNCTION()
	void OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& TargetData);

	/** Local machine only: one trigger pull traced and sent. */
	void FireShot();

	/** The repeat behind Auto and Burst, on the locally-controlled machine only. */
	void FireTick();

	/** AUTO-RELOAD (founder, 3 Sep). Pulling the trigger on an empty magazine reloads instead of
	 *  doing nothing. True only when the magazine is the reason the shot failed AND the reserve
	 *  can actually refill it — an empty reserve is a dry click, by the founder's call, not a
	 *  two-second reload that transfers nothing. */
	bool ShouldAutoReload(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** The seam that actually catches the empty magazine. CanActivateAbility runs CheckCost, so a
	 *  trigger pull with no round is REFUSED before ActivateAbility is ever entered — measured
	 *  3 Sep, which is why the first cut of this feature (hooked on the CommitAbility failure)
	 *  could never fire. */
	virtual void OnActivationRefused(const FGameplayAbilityActorInfo* ActorInfo) const override;

	/** Presses Input.Weapon.Reload on the ASC — the SAME door the player's key and the bot's
	 *  Verb_Reload go through, so both get this from one place and neither gets a private path. */
	void RequestAutoReload(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** Montage and muzzle cue — what every machine must see. */
	void PlayShotEffects();

	FTimerHandle FireTimer;

	/** Local: shots this trigger pull has sent, which is what Burst counts. */
	int32 ShotsFired = 0;

	/** Authority: claims accepted this activation. Shot 0 is the one CommitAbility already paid. */
	int32 ShotsJudged = 0;

	/** Authority: the rate floor's clock. */
	float LastAcceptedShotTime = 0.f;

	/** Built on first use, not in the constructor — see the definition. */
	mutable FGameplayTagContainer CooldownTags;

	FActiveGameplayEffectHandle FiringHandle;
};
