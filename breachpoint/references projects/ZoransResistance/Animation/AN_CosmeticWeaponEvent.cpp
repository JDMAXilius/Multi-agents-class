// Copyright Zollpa LLC


#include "AN_CosmeticWeaponEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "ZoransResistance/AbilitySystem/Data/ZoransGameplayTags.h"

void UAN_CosmeticWeaponEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	if (IsValid(MeshComp))
	{
		AActor* OwningActor = MeshComp->GetOwner();
		if (IsValid(OwningActor))
		{
			FGameplayEventData Payload = FGameplayEventData();
			Payload.EventMagnitude = EventIndex;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwningActor, GameplayTag_CosmeticWeaponEvent, Payload);
		}
	}
}
