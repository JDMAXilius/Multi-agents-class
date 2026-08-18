#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "BNAbilitySystemComponent.generated.h"

UCLASS()
class BREACHPOINTNEXT_API UBNAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);

	/** Cancels every RUNNING ability that the match freeze is meant to stop, leaving the ones that
	 *  ignore it (death, hit-react) alone. Refusing activation is not enough on its own: an Auto
	 *  weapon's fire ability keeps its own loop alive, so a player holding the trigger when the
	 *  match ends would go on killing through the whole post-match, server-authoritatively. */
	void CancelAbilitiesBlockedByFreeze();
};
