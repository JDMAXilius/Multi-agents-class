// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/Notifies/OSAnimNotify_ActionComboBuffer.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Data/OSGameplayTags.h"
#include "GameplayEffectTypes.h"

UOSAnimNotify_ActionComboBuffer::UOSAnimNotify_ActionComboBuffer()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor::Green;
#endif
}

void UOSAnimNotify_ActionComboBuffer::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp) return;
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner());
	if (!IsValid(ASC)) return;
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	if (Tags.Combo_CanQueue.IsValid())
	{
		ASC->AddLooseGameplayTag(Tags.Combo_CanQueue);
	}

	// Fire a GameplayEvent at window open so abilities can consume pre-buffered input immediately.
	if (Tags.Event_ComboWindowOpened.IsValid())
	{
		FGameplayEventData EventData;
		EventData.EventTag = Tags.Event_ComboWindowOpened;
		EventData.Instigator = MeshComp->GetOwner();
		EventData.Target = MeshComp->GetOwner();
		ASC->HandleGameplayEvent(Tags.Event_ComboWindowOpened, &EventData);
	}
}

void UOSAnimNotify_ActionComboBuffer::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (!MeshComp) return;
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner());
	if (!IsValid(ASC)) return;
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	if (Tags.Combo_CanQueue.IsValid())
	{
		ASC->RemoveLooseGameplayTag(Tags.Combo_CanQueue);
	}
}
