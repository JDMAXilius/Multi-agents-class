#include "AbilitySystem/Abilities/BNGA_ADS.h"

#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Weapons/BNWeapon.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Actor.h"

bool UBNGA_ADS::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Sprint wins, one direction only. Refusing here — on client and server alike, since this runs
	// on both — is also what keeps the two speed multipliers from ever stacking.
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || ASC->HasMatchingGameplayTag(BNTags::State_Movement_Sprinting))
	{
		return false;
	}

	// The row decides, per weapon: a knife does not aim, and neither does an empty hand.
	const ABNCharacter* Character = Cast<ABNCharacter>(ActorInfo->AvatarActor.Get());
	const UBNEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	const ABNWeapon* Weapon = Equipment ? Equipment->GetCurrentWeapon() : nullptr;
	const FBNWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	return Row && Row->bCanADS;
}

void UBNGA_ADS::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The state and the speed, together — sprint's SetSprintActive with no gate between press and
	// state. The tag is what the anim instance (pose, FOV) and the sim proxies read.
	ADSHandle = ApplyStateTag(BNTags::State_Weapon_ADS);
	const FGameplayEffectSpecHandle SpeedSpec = MakeOutgoingGameplayEffectSpec(UBNGE_ADS::StaticClass(), 1.f);
	if (SpeedSpec.IsValid())
	{
		SpeedHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpeedSpec);
	}

	// Descope: the tag's arrival cancels the aim. Registered per-instance and unregistered in
	// EndAbility — the anim instance's own listener pattern.
	LastRecentDamageCount = ASC->GetTagCount(BNTags::State_Combat_RecentDamage);
	RecentDamageHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Combat_RecentDamage, EGameplayTagEventType::AnyCountChange)
		.AddUObject(this, &UBNGA_ADS::OnRecentDamageChanged);
	SprintHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Movement_Sprinting, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UBNGA_ADS::OnSprintChanged);

	if (AActor* Avatar = ActorInfo->AvatarActor.Get())
	{
		Avatar->OnDestroyed.AddDynamic(this, &UBNGA_ADS::OnAvatarDestroyed);
	}

	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	ReleaseTask->OnRelease.AddDynamic(this, &UBNGA_ADS::OnInputRelease);
	ReleaseTask->ReadyForActivation();
}

void UBNGA_ADS::OnInputRelease(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBNGA_ADS::OnRecentDamageChanged(const FGameplayTag Tag, int32 NewCount)
{
	// DESCOPE, on count INCREASE only — see the header for why neither NewOrRemoved nor a plain
	// NewCount > 0 check survives overlapping damage windows. Cancelled, not completed: the player
	// did not choose to stop aiming, the key is still held, and re-aim is a re-press.
	const bool bHitLanded = NewCount > LastRecentDamageCount;
	LastRecentDamageCount = NewCount;
	if (bHitLanded)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UBNGA_ADS::OnSprintChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Sprint wins, both directions now — see the header. Cancelled like descope: the ADS key is
	// still held, and releasing sprint then re-pressing aim is the deliberate re-entry.
	if (NewCount > 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UBNGA_ADS::OnAvatarDestroyed(AActor* DestroyedActor)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UBNGA_ADS::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (RecentDamageHandle.IsValid())
		{
			ASC->UnregisterGameplayTagEvent(RecentDamageHandle, BNTags::State_Combat_RecentDamage, EGameplayTagEventType::AnyCountChange);
			RecentDamageHandle.Reset();
		}
		if (SprintHandle.IsValid())
		{
			ASC->UnregisterGameplayTagEvent(SprintHandle, BNTags::State_Movement_Sprinting, EGameplayTagEventType::NewOrRemoved);
			SprintHandle.Reset();
		}
		if (SpeedHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(SpeedHandle);
			SpeedHandle = FActiveGameplayEffectHandle();
		}
	}
	if (AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)
	{
		Avatar->OnDestroyed.RemoveDynamic(this, &UBNGA_ADS::OnAvatarDestroyed);
	}
	RemoveStateTag(ADSHandle);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
