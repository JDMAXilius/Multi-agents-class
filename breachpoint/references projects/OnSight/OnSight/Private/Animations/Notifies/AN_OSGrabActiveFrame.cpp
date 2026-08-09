#include "Animations/Notifies/AN_OSGrabActiveFrame.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Data/OSGameplayTags.h"

void UAN_OSGrabActiveFrame::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// No authority gate — LocalPredicted abilities run this on both server and owning client.
	// Simulated proxies see the notify fire too but have no subscribed UGA_OSGrab instance on
	// their ASC, so HandleGameplayEvent is a no-op for them.

	const FGameplayTag& Tag = FOSGameplayTags::Get().Event_Grab_ActiveFrame;

	FGameplayEventData Payload;
	Payload.EventTag = Tag;
	Payload.Instigator = Owner;
	Payload.Target = Owner;

	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner))
	{
		ASC->HandleGameplayEvent(Tag, &Payload);
	}
}
