#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "BNGA_Death.generated.h"

/**
 * Death. Activated BY CLASS on the authority when UBNHealthComponent reports Health at zero —
 * not by a key, and never by a client. ServerOnly, unlike every other BN ability: death is not
 * intent, it is a verdict, and there is nothing for the dying machine to predict.
 *
 * It holds State.Dead on a GE for as long as the life lasts (Mixed replication carries that tag
 * to the owner and to simulated proxies, so death is visible on every machine as STATE and never
 * as a local bool), cancels every other ability, and asks the game mode for the respawn. The
 * respawn cancels this ability, and that is what takes the tag off — one owner, start to finish.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGA_Death : public UBNGameplayAbility
{
	GENERATED_BODY()

public:
	UBNGA_Death();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	FActiveGameplayEffectHandle DeadHandle;
};
