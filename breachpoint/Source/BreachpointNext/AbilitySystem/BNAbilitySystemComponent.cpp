#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "BreachpointNext.h"
#include "Abilities/GameplayAbility.h"

void UBNAbilitySystemComponent::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	bool bAnySpecListens = false;
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}
		bAnySpecListens = true;

		AbilitySpecInputPressed(Spec);
		if (Spec.IsActive())
		{
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle,
				Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey());
		}
		else
		{
			// THE ONE PLACE every input-driven ability's activation can be seen, verdict included.
			// ADS, melee, grenade, fire, swap, lean, sprint all pass through here — so "I pressed
			// it and nothing happened" stops being a mystery for ALL of them at once, rather than
			// each ability needing its own instrumentation.
			const bool bActivated = TryActivateAbility(Spec.Handle);
			UE_LOG(LogBN, Log, TEXT("BNInput: %s -> %s : %s"),
				*InputTag.ToString(), *GetNameSafe(Spec.Ability),
				bActivated ? TEXT("ACTIVATED") : TEXT("REFUSED (blocked by tags, cost/cooldown, dead, or CanActivate said no)"));
		}
	}

	// The gap between "the key works" and "the ability exists": the press crossed the input assets
	// and reached the ASC, and then went nowhere. Without this line that outcome is silent and
	// reads exactly like a dead key — with it, the log names which half of the chain to fix.
	if (!bAnySpecListens)
	{
		UE_LOG(LogBN, Warning, TEXT("BNASC: input tag %s reached the ASC but NO granted ability carries it — the grant is missing (or defaults are not granted yet)."),
			*InputTag.ToString());
	}
}

void UBNAbilitySystemComponent::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		AbilitySpecInputReleased(Spec);
		if (Spec.IsActive())
		{
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle,
				Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey());
		}
	}
}
