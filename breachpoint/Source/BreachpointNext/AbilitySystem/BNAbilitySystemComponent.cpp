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
			TryActivateAbility(Spec.Handle);
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
