#include "AbilitySystem/Abilities/BNGA_Equip.h"

#include "Characters/BNCharacter.h"
#include "Weapons/BNEquipmentComponent.h"

UBNGA_Equip::UBNGA_Equip()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBNGA_Equip::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const ABNCharacter* Character = ActorInfo ? Cast<ABNCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (UBNEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr)
	{
		ExecuteEquip(Equipment);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UBNGA_Equip::ExecuteEquip(UBNEquipmentComponent* Equipment)
{
	if (EquipSlot != INDEX_NONE)
	{
		Equipment->EquipIndex(EquipSlot);
	}
}

void UBNGA_SwapNext::ExecuteEquip(UBNEquipmentComponent* Equipment)
{
	Equipment->EquipNext();
}

void UBNGA_SwapPrevious::ExecuteEquip(UBNEquipmentComponent* Equipment)
{
	Equipment->EquipPrevious();
}
