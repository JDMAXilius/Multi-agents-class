// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/Notifies/AN_ApplyTagState.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

void UAN_ApplyTagState::NotifyBegin( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                  const FAnimNotifyEventReference& EventReference )
{
	if (!MeshComp) return;
	ApplyState(MeshComp->GetOwner(), true);
}

void UAN_ApplyTagState::NotifyEnd( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference )
{
	if (!MeshComp) return;
	ApplyState(MeshComp->GetOwner(), false);
}

void UAN_ApplyTagState::ApplyState( AActor* owner, const bool toAdd ) const
{
	if (!owner) return;
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(owner))
	{
		if (toAdd)
			ASC->AddLooseGameplayTag(StateTag);
		else
			ASC->RemoveLooseGameplayTag(StateTag);
	}
}
