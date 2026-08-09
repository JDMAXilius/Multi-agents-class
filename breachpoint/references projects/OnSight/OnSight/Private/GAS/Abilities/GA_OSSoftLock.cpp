#include "GAS/Abilities/GA_OSSoftLock.h"

#include "AbilitySystemComponent.h"
#include "Characters/OSCharacter.h"
#include "Data/OSGameplayTags.h"
#include "GAS/Effects/GE_OSTracking.h"

/** Same HUD hook as UGA_OSAttack: GenericGameplayEventCallbacks listen for UI_TargetChanged. */
static void BroadcastSoftLockTargetForUI(UGameplayAbility* Ability, AActor* Target)
{
	if (UAbilitySystemComponent* ASC = Ability->GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayEventData Payload;
		Payload.Target = Target;
		ASC->HandleGameplayEvent(FOSGameplayTags::Get().UI_TargetChanged, &Payload);
	}
}

UGA_OSSoftLock::UGA_OSSoftLock()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;//
	TrackingStateTag = FGameplayTag::RequestGameplayTag(TEXT("Character.State.Tracking"));
	TrackingEffectClass = UGE_OSTracking::StaticClass();
}

void UGA_OSSoftLock::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* OSC_ASC = GetAbilitySystemComponentFromActorInfo();
	if (!OSC_ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bTracking = OSC_ASC->HasMatchingGameplayTag(TrackingStateTag);
	AOSCharacter* OwningChar = Cast<AOSCharacter>(GetAvatarActorFromActorInfo());

	if (bTracking)
	{
		RemoveTrackingEffect(OSC_ASC);
		BroadcastSoftLockTargetForUI(this, nullptr);
	}
	else
	{
		ApplyTrackingEffect(OSC_ASC);
		BroadcastSoftLockTargetForUI(this, OwningChar ? OwningChar->SoftLockTarget : nullptr);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_OSSoftLock::RemoveTrackingEffect(UAbilitySystemComponent* ASC)
{
	FGameplayTagContainer Tags;
	Tags.AddTag(TrackingStateTag);
	ASC->RemoveActiveEffectsWithGrantedTags(Tags);
}

void UGA_OSSoftLock::ApplyTrackingEffect(UAbilitySystemComponent* ASC)
{
	if (!ASC || !TrackingEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
		TrackingEffectClass, TrackingEffectLevel, Context);

	if (Spec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}
