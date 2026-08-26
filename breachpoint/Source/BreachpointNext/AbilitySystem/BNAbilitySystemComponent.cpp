#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "BreachpointNext.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "Abilities/GameplayAbility.h"
#include "Core/BNGameplayTags.h"

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
		// BN20's separating facts ride the line (its step 1: name the cause with
		// timestamps, never infer it): dead=yes classifies a press from the corpse
		// window, dead=no with a fresh avatar classifies a spawn-grant race, and the
		// avatar name plus the log's own timestamp is what correlates either against
		// the grant moment. 308-to-11 stays anonymous without this.
		UE_LOG(LogBN, Warning, TEXT("BNASC: input tag %s reached the ASC but NO granted ability carries it — the grant is missing (or defaults are not granted yet). dead=%s avatar=%s"),
			*InputTag.ToString(),
			HasMatchingGameplayTag(BNTags::State_Dead) ? TEXT("yes") : TEXT("no"),
			*GetNameSafe(GetAvatarActor()));
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

void UBNAbilitySystemComponent::CancelAbilitiesBlockedByFreeze()
{
	// Collected under the lock, cancelled outside it: cancelling mutates the same list.
	TArray<FGameplayAbilitySpecHandle> ToCancel;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			const UBNGameplayAbility* Ability = Cast<UBNGameplayAbility>(Spec.Ability);
			if (Spec.IsActive() && Ability && !Ability->IgnoresMatchFreeze())
			{
				ToCancel.Add(Spec.Handle);
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : ToCancel)
	{
		CancelAbilityHandle(Handle);
	}
}
