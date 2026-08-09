#include "GAS/Abilities/GA_OSRecoil.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Data/OSGameplayTags.h"

UGA_OSRecoil::UGA_OSRecoil()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	// Asset tag (used by CancelAbilitiesWithTag from other reactive abilities)
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Recoil);
	SetAssetTags(AssetTags);

	// Trigger off GameplayEvent.Effect.Recoil (matches GA_OSHitReaction pattern)
	FAbilityTriggerData Trigger;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	Trigger.TriggerTag = Tags.Event_Recoil;
	AbilityTriggers.Add(Trigger);

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	// State tag active during recoil (replicates to clients via ASC)
	ActivationOwnedTags.AddTag(Tags.IsRecoiling);

	// Prevent double activation, dead guard, and montage conflict with hit reaction
	ActivationBlockedTags.AddTag(Tags.IsRecoiling);
	ActivationBlockedTags.AddTag(Tags.IsDead);
	ActivationBlockedTags.AddTag(Tags.IsHitReacting);

	// Cancel active combat abilities (kills combo, charged attack, grab).
	// BP combos use Attack.Light / Attack.Heavy asset tags — Attack is the parent that matches both.
	CancelAbilitiesWithTag.AddTag(Tags.Attack);
	// Charge is deprecated
	CancelAbilitiesWithTag.AddTag(Tags.Ability_ChargedAttack);
	CancelAbilitiesWithTag.AddTag(Tags.Ability_Grab);

	// Full lockout during recoil (attack, block, dodge, sprint — all blocked)
	BlockAbilitiesWithTag.AddTag(Tags.Ability);
}

void UGA_OSRecoil::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!RecoilMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, RecoilMontage, 1.f, NAME_None, true);

	MontageTask->OnCompleted.AddDynamic(this, &UGA_OSRecoil::OSEndAbility);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_OSRecoil::OSEndAbility);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_OSRecoil::OSCancelAbility);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_OSRecoil::OSCancelAbility);
	MontageTask->ReadyForActivation();
}

void UGA_OSRecoil::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	MontageTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
