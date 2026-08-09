#include "Animations/Notifies/AN_OSDirectDamage.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Data/OSGameplayTags.h"

void UAN_OSDirectDamage::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	const FGameplayTag& Tag = FOSGameplayTags::Get().Event_DirectDamage;

	FGameplayEventData Payload;
	Payload.EventTag = Tag;
	Payload.Instigator = Owner;
	Payload.Target = Owner;

	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner))
	{
		ASC->HandleGameplayEvent(Tag, &Payload);
	}
}
