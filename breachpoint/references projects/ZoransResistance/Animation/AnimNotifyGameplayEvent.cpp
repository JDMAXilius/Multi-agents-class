// Copyright Zollpa LLC


#include "ZoransResistance/Animation/AnimNotifyGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"

void UAnimNotifyGameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (IsValid(MeshComp))
	{
		AActor* OwningActor = MeshComp->GetOwner();
		if (IsValid(OwningActor))
		{
			const FGameplayEventData Payload = FGameplayEventData();
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwningActor, EventTag, Payload);
		}
	}
}
