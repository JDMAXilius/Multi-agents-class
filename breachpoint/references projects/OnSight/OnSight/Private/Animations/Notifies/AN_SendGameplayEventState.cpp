// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/Notifies/AN_SendGameplayEventState.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/OSCharacter.h"

void UAN_SendGameplayEventState::NotifyBegin( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                              float TotalDuration, const FAnimNotifyEventReference& EventReference )
{
	if (!IsValid(MeshComp)) return;
	auto owner = MeshComp->GetOwner();
	if (!IsValid(owner)) return;
	SendEventFromTag(owner, OnBeginTag);
}

void UAN_SendGameplayEventState::NotifyEnd( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference )
{
	if (!IsValid(MeshComp)) return;
	auto owner = MeshComp->GetOwner();
	if (!IsValid(owner)) return;
	SendEventFromTag(owner, OnEndTag);
}

void UAN_SendGameplayEventState::SendEventFromTag( AActor* owner, const FGameplayTag& tag ) const
{
	if (!IsValid(owner))
	{
		return;
	}
	if (owner->IsActorBeingDestroyed())
	{
		return;
	}
	if (!tag.IsValid())
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = tag;
	Payload.Instigator = owner;
	Payload.Target = owner;

	// MP GAS best-practice: route via ASC directly if present (PlayerState-owned ASC safe).
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(owner))
	{
		if (!IsValid(ASC))
		{
			return;
		}
		if (AActor* ASCOwner = ASC->GetOwner())
		{
			if (!IsValid(ASCOwner) || ASCOwner->IsActorBeingDestroyed())
			{
				return;
			}
		}
		// Guard: HandleGameplayEvent expects ASC to be initialized with a valid ActorInfo.
		// During teardown / montage cancellation, notifies can still fire while ASC is partially uninitialized.
		const FGameplayAbilityActorInfo* Info = ASC->AbilityActorInfo.Get();
		// Require BOTH owner+avatar: HandleGameplayEvent -> TriggerAbilityFromGameplayEvent paths assume a fully initialized ActorInfo.
		if (!Info || !Info->OwnerActor.IsValid() || !Info->AvatarActor.IsValid())
		{
			return;
		}
		// Notify is fired from the avatar's mesh; if the ASC has a different avatar, ignore.
		if (Info->AvatarActor.Get() != owner)
		{
			return;
		}
		ASC->HandleGameplayEvent(tag, &Payload);
	}
	else
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(owner, tag, Payload);
	}
}
