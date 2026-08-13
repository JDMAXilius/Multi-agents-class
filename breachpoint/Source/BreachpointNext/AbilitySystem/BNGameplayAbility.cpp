#include "AbilitySystem/BNGameplayAbility.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystemComponent.h"

UBNGameplayAbility::UBNGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// The dead activate nothing. Here rather than as ActivationBlockedTags in the constructor for the
// reason UBNGA_Lean resolves its tag in a virtual — native tags are not guaranteed registered
// while CDOs are built. UBNGA_Death itself activates BEFORE the tag exists, and is refused after.
//
// What this check removes is the ACTIVATION, on both roles. It does not close the replication
// window: State.Dead reaches the owning client one hop after the server applies it, so an input
// inside that hop activates locally, plays its predicted cue, and is then refused server-side —
// residue is a muzzle flash the server never fired. Symmetric on respawn, and cosmetic only.
bool UBNGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return !ASC || !ASC->HasMatchingGameplayTag(BNTags::State_Dead);
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
