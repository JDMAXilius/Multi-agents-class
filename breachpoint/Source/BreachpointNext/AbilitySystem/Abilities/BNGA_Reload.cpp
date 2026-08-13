#include "AbilitySystem/Abilities/BNGA_Reload.h"

#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Weapons/BNWeapon.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	ABNWeapon* BNReloadGetWeapon(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const ABNCharacter* Character = ActorInfo ? Cast<ABNCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		const UBNEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
		return Equipment ? Equipment->GetCurrentWeapon() : nullptr;
	}
}

bool UBNGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || ASC->HasMatchingGameplayTag(BNTags::State_Weapon_Reloading))
	{
		return false;
	}

	const ABNWeapon* Weapon = BNReloadGetWeapon(ActorInfo);
	return Weapon && Weapon->GetCurrentAmmo() < Weapon->GetMagazineSize();
}

void UBNGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const ABNWeapon* Weapon = BNReloadGetWeapon(ActorInfo);
	const FBNWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !Row)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The state tag, through a GE like every other state in this module — so it reaches simulated
	// proxies and so UBNGA_Fire::CanActivateAbility can read it off the ASC on either machine.
	ReloadingHandle = ApplyStateTag(BNTags::State_Weapon_Reloading);

	if (UAnimMontage* Montage = Row->ReloadMontage.IsNull() ? nullptr : Row->ReloadMontage.LoadSynchronous())
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Montage, 1.f, NAME_None, /*bStopWhenAbilityEnds=*/true);
		Task->OnCompleted.AddDynamic(this, &UBNGA_Reload::OnReloadFinished);
		Task->OnBlendOut.AddDynamic(this, &UBNGA_Reload::OnReloadFinished);
		Task->OnInterrupted.AddDynamic(this, &UBNGA_Reload::OnReloadInterrupted);
		Task->OnCancelled.AddDynamic(this, &UBNGA_Reload::OnReloadInterrupted);
		Task->ReadyForActivation();
		return;
	}

	// No ReloadMontage on the row: the montages are ANNOUNCED assets, not created by this packet.
	// Reload still works, timed by the row's own ReloadTime, rather than being a dead key until
	// the asset lands — and it takes the identical commit path, so nothing changes when it does.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FallbackTimer, this, &UBNGA_Reload::OnReloadFinished, FMath::Max(0.1f, Row->ReloadTime), /*bLoop=*/false);
		return;
	}

	OnReloadFinished();
}

void UBNGA_Reload::OnReloadFinished()
{
	// COMPLETION is the commit point, not a notify, and the record is why: the template's reload
	// montages carry no reload notify at all — the only one in evidence is AN_FPST_Melee
	// (MyCharacter.cpp:1267-1276) — so committing on a notify would depend on an asset that does
	// not exist. Completion needs none, and moving to a notify later changes only this line.
	if (ABNWeapon* Weapon = BNReloadGetWeapon(CurrentActorInfo))
	{
		// Authority-only inside Reload(); every other machine sees CurrentAmmo replicate.
		Weapon->Reload();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBNGA_Reload::OnReloadInterrupted()
{
	// Cancelled: no refill. bWasCancelled is the record of it, and it is what a swap mid-reload
	// produces via the spec being cleared.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UBNGA_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallbackTimer);
	}
	RemoveStateTag(ReloadingHandle);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
