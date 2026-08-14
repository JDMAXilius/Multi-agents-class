#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "BNGA_ADS.generated.h"

/**
 * Aim down sights. LocalPredicted, held while Input.Weapon.ADS is down, granted by the PlayerState
 * (a body verb — it must survive weapon swaps and exist whatever the ability-set config says).
 *
 * Sprint's shape on purpose, minus the gate: ADS has no forward-intent condition, so the ability IS
 * the state — activation applies State.Weapon.ADS (Mixed replication carries it to sim proxies,
 * whose anim layers select the ADS pose off GameplayTag_IsADS) and the UBNGE_ADS speed multiply;
 * EndAbility removes both. The FOV blend is not here — it is the anim instance's, owner-only,
 * reading the same tag.
 *
 * DESCOPE, the founder's Halo ruling: a landed hit knocks you out of ADS. State.Combat.RecentDamage
 * arrives on every point of damage (its shields-off skip was removed the moment this — its second
 * reader — existed, exactly as that gate's comment scheduled), and this ability cancels itself on
 * the tag's arrival. Re-pressing re-aims; Halo 4's mistake (aim punch instead of descope) stays
 * declined.
 *
 * Refusals, in CanActivateAbility: dead (base class), sprinting (one-directional exclusion — sprint
 * wins, and the exclusion is also what keeps the two speed multipliers from stacking), and a
 * current weapon whose row says bCanADS false (a knife does not aim) or no weapon at all.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGA_ADS : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnInputRelease(float TimeHeld);

	/** Descope. Fires on both the owning client's and the authority's instance — each cancels its
	 *  own copy, the same way both copies of any LocalPredicted ability end. */
	void OnRecentDamageChanged(const FGameplayTag Tag, int32 NewCount);

	/** "Sprint wins", made real. CanActivateAbility refuses ADS while sprinting, but that guards
	 *  only ONE direction — sprint has no ADS check, so starting a sprint mid-aim would have run
	 *  BOTH speed multipliers (0.4167 x 1.5) with the gun still raised. The sprint tag's arrival
	 *  cancels ADS, exactly as descope does, and the exclusion claim becomes true both ways. */
	void OnSprintChanged(const FGameplayTag Tag, int32 NewCount);

	/** Travel and plain destruction kill the avatar without a death — jump's lesson. Without this,
	 *  the ADS state GE would sit on the persistent ASC until the next death's sweep. */
	UFUNCTION()
	void OnAvatarDestroyed(AActor* DestroyedActor);

	FActiveGameplayEffectHandle ADSHandle;
	FActiveGameplayEffectHandle SpeedHandle;
	FDelegateHandle RecentDamageHandle;
	FDelegateHandle SprintHandle;
};
