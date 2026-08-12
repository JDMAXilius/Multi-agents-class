#include "AbilitySystem/BNGameplayAbility.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "AbilitySystemComponent.h"

UBNGameplayAbility::UBNGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

FActiveGameplayEffectHandle UBNGameplayAbility::ApplyStateTag(FGameplayTag Tag)
{
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(UBNGE_State::StaticClass(), 1.f);
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	SpecHandle.Data->DynamicGrantedTags.AddTag(Tag);
	return ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
}

void UBNGameplayAbility::RemoveStateTag(FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	Handle = FActiveGameplayEffectHandle();
}
