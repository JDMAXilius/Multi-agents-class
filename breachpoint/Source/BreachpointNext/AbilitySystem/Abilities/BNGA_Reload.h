#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "Engine/TimerHandle.h"
#include "BNGA_Reload.generated.h"

/**
 * Reload. LocalPredicted, input tag Input.Weapon.Reload, granted by the WEAPON's ability set
 * beside Fire — neither revokes itself by running, which is what makes weapon-granting correct
 * for them and what the row's AbilitySet column is for.
 *
 * Blocked while already reloading and while the magazine is already full. Cancelled by SWAP, and
 * for free: revoking the weapon's set clears this spec, which cancels the running instance, and a
 * cancel clears the commit timer so the refill never lands. NOT cancelled by fire — fire is
 * blocked instead, see UBNGA_Fire::CanActivateAbility for why.
 *
 * The ammo commit is the AUTHORITY's timer, never the montage's completion, and the lifetime is
 * the authority's too — see ActivateAbility for the race that forces both.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGA_Reload : public UBNGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Authority only: the magazine refills here, and the ability ends here. */
	void OnReloadCommitted();

	FActiveGameplayEffectHandle ReloadingHandle;

	/** Authority only. Cleared by EndAbility, so a cancelled reload never refills. */
	FTimerHandle CommitTimer;
};
