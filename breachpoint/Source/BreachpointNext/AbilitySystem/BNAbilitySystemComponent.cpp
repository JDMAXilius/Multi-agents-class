#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

void UBNAbilitySystemComponent::AbilityInputTagPressed(FGameplayTag InputTag)
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
