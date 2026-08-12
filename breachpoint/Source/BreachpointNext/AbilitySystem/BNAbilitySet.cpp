#include "AbilitySystem/BNAbilitySet.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

void UBNAbilitySet::GiveToAbilitySystem(UBNAbilitySystemComponent* ASC, FBNAbilitySetHandles& OutHandles) const
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FBNAbilitySetAbility& Entry : Abilities)
	{
		if (!Entry.Ability)
		{
			continue;
		}

		FGameplayAbilitySpec Spec(Entry.Ability, Entry.Level);
		Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		OutHandles.AbilityHandles.Add(ASC->GiveAbility(Spec));
	}

	for (const TSubclassOf<UGameplayEffect>& Effect : Effects)
	{
		if (!Effect)
		{
			continue;
		}

		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(Effect, 1.f, ASC->MakeEffectContext());
		if (SpecHandle.IsValid())
		{
			OutHandles.EffectHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()));
		}
	}
}

void UBNAbilitySet::TakeFromAbilitySystem(UBNAbilitySystemComponent* ASC, FBNAbilitySetHandles& Handles) const
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : Handles.AbilityHandles)
	{
		ASC->ClearAbility(Handle);
	}
	for (const FActiveGameplayEffectHandle& Handle : Handles.EffectHandles)
	{
		ASC->RemoveActiveGameplayEffect(Handle);
	}

	Handles.AbilityHandles.Reset();
	Handles.EffectHandles.Reset();
}
