// BREACHPOINT — BP03 step 2. Reload and swap: the other half of the fire path.
#include "AbilitySystem/Abilities/BRGA_WeaponUtility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Data/BRDataRows.h"
#include "GameFramework/Actor.h"
#include "Weapons/BREquipmentComponent.h"
#include "Weapons/BRWeaponInstance.h"

namespace
{
	UBREquipmentComponent* FindEquipment(const AActor* Avatar)
	{
		return Avatar ? Avatar->FindComponentByClass<UBREquipmentComponent>() : nullptr;
	}

	UBRWeaponInstance* FindActiveWeapon(const AActor* Avatar)
	{
		const UBREquipmentComponent* Equipment = FindEquipment(Avatar);
		return Equipment ? Equipment->GetActiveWeapon() : nullptr;
	}

	float FallbackSeconds(const FBRWeaponRow* Row, bool bIsReload)
	{
		if (!Row)
		{
			return 0.f;
		}
		return bIsReload ? Row->ReloadTime_s : Row->EquipTime_s;
	}
}

UBRGA_Reload::UBRGA_Reload(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Weapon_Reload);
	SetAssetTags(AssetTags);

	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	bCommitOnActivate = true;
}

bool UBRGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!BRGas::IsStageEnabled(EBRGasStage::Weapons))
	{
		return false;
	}

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UBRWeaponInstance* Weapon = FindActiveWeapon(GetAvatarActorFromActorInfo());
	const FBRWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	if (!Weapon || !Row)
	{
		return false;
	}

	return UBRWeaponInstance::CalcReloadTransfer(Row->MagSize, Weapon->GetAmmoInMag(), Weapon->GetAmmoReserve()) > 0;
}

void UBRGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bCommitted = false;

	if (UAbilityTask_WaitGameplayEvent* WaitCommit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, BRGameplayTags::Event_Weapon_ReloadCommit))
	{
		WaitCommit->EventReceived.AddDynamic(this, &UBRGA_Reload::OnReloadCommit);
		WaitCommit->ReadyForActivation();
	}

	const UBRWeaponInstance* Weapon = FindActiveWeapon(GetAvatarActorFromActorInfo());
	const float Seconds = FallbackSeconds(Weapon ? Weapon->GetRow() : nullptr, true);

	if (Seconds <= 0.f)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Reload: ReloadTime_s is 0 and no montage raised %s — committing immediately."),
			*BRGameplayTags::Event_Weapon_ReloadCommit.GetTag().GetTagName().ToString());
		Commit();
		return;
	}

	if (UAbilityTask_WaitDelay* Fallback = UAbilityTask_WaitDelay::WaitDelay(this, Seconds))
	{
		Fallback->OnFinish.AddDynamic(this, &UBRGA_Reload::OnFallbackElapsed);
		Fallback->ReadyForActivation();
	}
}

void UBRGA_Reload::OnReloadCommit(FGameplayEventData Payload)
{
	Commit();
}

void UBRGA_Reload::OnFallbackElapsed()
{
	if (!bCommitted)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Reload committed on the ReloadTime_s timer, not on %s. No reload montage "
				 "exists yet (BP18 owns Content); the notify is authoritative once it does."),
			*BRGameplayTags::Event_Weapon_ReloadCommit.GetTag().GetTagName().ToString());
	}
	Commit();
}

void UBRGA_Reload::Commit()
{
	if (bCommitted)
	{
		return;
	}
	bCommitted = true;

	if (HasAuthority(&CurrentActivationInfo))
	{
		if (UBRWeaponInstance* Weapon = FindActiveWeapon(GetAvatarActorFromActorInfo()))
		{
			const int32 Moved = Weapon->CommitReload();
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

UBRGA_WeaponSwap::UBRGA_WeaponSwap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Weapon_Swap);
	SetAssetTags(AssetTags);

	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	bCommitOnActivate = true;
}

bool UBRGA_WeaponSwap::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!BRGas::IsStageEnabled(EBRGasStage::Weapons))
	{
		return false;
	}

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UBREquipmentComponent* Equipment = FindEquipment(GetAvatarActorFromActorInfo());
	if (!Equipment)
	{
		return false;
	}

	const int32 ActiveSlot = Equipment->GetActiveSlotIndex();
	const EBRWeaponSlot OtherSlot = (ActiveSlot == 0) ? EBRWeaponSlot::Secondary : EBRWeaponSlot::Primary;
	return Equipment->IsSlotOccupied(OtherSlot);
}

void UBRGA_WeaponSwap::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bCommitted = false;

	if (UAbilityTask_WaitGameplayEvent* WaitCommit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, BRGameplayTags::Event_Weapon_SwapCommit))
	{
		WaitCommit->EventReceived.AddDynamic(this, &UBRGA_WeaponSwap::OnSwapCommit);
		WaitCommit->ReadyForActivation();
	}

	const UBRWeaponInstance* Weapon = FindActiveWeapon(GetAvatarActorFromActorInfo());
	const float Seconds = FallbackSeconds(Weapon ? Weapon->GetRow() : nullptr, false);

	if (Seconds <= 0.f)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_WeaponSwap: EquipTime_s is 0 and no montage raised %s — committing "
				 "immediately. R3 balances the sandbox around a 0.4 s swap, so a 0 here is a data "
				 "gap, not a fast weapon."),
			*BRGameplayTags::Event_Weapon_SwapCommit.GetTag().GetTagName().ToString());
		Commit();
		return;
	}

	if (UAbilityTask_WaitDelay* Fallback = UAbilityTask_WaitDelay::WaitDelay(this, Seconds))
	{
		Fallback->OnFinish.AddDynamic(this, &UBRGA_WeaponSwap::OnFallbackElapsed);
		Fallback->ReadyForActivation();
	}
}

void UBRGA_WeaponSwap::OnSwapCommit(FGameplayEventData Payload)
{
	Commit();
}

void UBRGA_WeaponSwap::OnFallbackElapsed()
{
	if (!bCommitted)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_WeaponSwap committed on the EquipTime_s timer, not on %s. No swap montage "
				 "exists yet (BP18 owns Content)."),
			*BRGameplayTags::Event_Weapon_SwapCommit.GetTag().GetTagName().ToString());
	}
	Commit();
}

void UBRGA_WeaponSwap::Commit()
{
	if (bCommitted)
	{
		return;
	}
	bCommitted = true;

	if (IsLocallyControlled())
	{
		if (UBREquipmentComponent* Equipment = FindEquipment(GetAvatarActorFromActorInfo()))
		{
			const int32 ActiveSlot = Equipment->GetActiveSlotIndex();
			const uint8 TargetSlot = (ActiveSlot == 0) ? 1 : 0;
			Equipment->ServerRequestSwapSlot(TargetSlot);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
