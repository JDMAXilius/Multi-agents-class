#include "FPS/BRAnimNotify_GameplayEvent.h"

#include "Components/SkeletalMeshComponent.h"

#include "FPS/BRAnimInstance.h"

UBRAnimNotify_GameplayEvent::UBRAnimNotify_GameplayEvent()
{
#if WITH_EDITORONLY_DATA
	// Gameplay-bearing, so it should not read like a footstep in a crowded montage track.
	NotifyColor = FColor(220, 90, 40);
#endif
}

FString UBRAnimNotify_GameplayEvent::GetNotifyName_Implementation() const
{
	// The track shows the TAG. A row reading "Event.Weapon.ReloadCommit" is reviewable at a
	// glance; one reading "BR Gameplay Event" three times over is not.
	return EventTag.IsValid() ? EventTag.ToString() : TEXT("BR Gameplay Event (NO TAG SET)");
}

void UBRAnimNotify_GameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Routed through the spine rather than sending from here, deliberately. Both gates -- one
	// mesh speaks for the pawn, and only the authority or the predicting owner sends -- live in
	// `SendSeamGameplayEvent`. A notify that sent its own event would bypass both and would
	// double or mis-fire exactly the way this seam already failed once.
	if (UBRAnimInstance* Spine = Cast<UBRAnimInstance>(MeshComp->GetAnimInstance()))
	{
		Spine->SendSeamGameplayEvent(EventTag);
	}
}
