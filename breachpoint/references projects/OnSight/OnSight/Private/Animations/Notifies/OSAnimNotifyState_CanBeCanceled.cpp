#include "Animations/Notifies/OSAnimNotifyState_CanBeCanceled.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

// TO BE DELETED

static UAbilitySystemComponent* GetASCFromMesh(USkeletalMeshComponent* MeshComp)
{
	return MeshComp ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()) : nullptr;
}

void UOSAnimNotifyState_CanBeCanceled::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UAbilitySystemComponent* ASC = GetASCFromMesh(MeshComp);
	if (!ASC || CancelPermissionTags.IsEmpty())
	{
		return;
	}

	for (const FGameplayTag& Tag : CancelPermissionTags)
	{
		if (Tag.IsValid())
		{
			ASC->AddLooseGameplayTag(Tag);
		}
	}
}

void UOSAnimNotifyState_CanBeCanceled::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	UAbilitySystemComponent* ASC = GetASCFromMesh(MeshComp);
	if (!ASC || CancelPermissionTags.IsEmpty())
	{
		return;
	}

	for (const FGameplayTag& Tag : CancelPermissionTags)
	{
		if (Tag.IsValid())
		{
			ASC->RemoveLooseGameplayTag(Tag);
		}
	}
}

