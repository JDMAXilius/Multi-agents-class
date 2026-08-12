#include "Animations/BRAnimNotifyState_GameplayEventWindow.h"

#include "Components/SkeletalMeshComponent.h"

#include "Animations/BRAnimInstance.h"

namespace
{
	void SendThrough(USkeletalMeshComponent* MeshComp, const FGameplayTag& Tag)
	{
		if (!MeshComp)
		{
			return;
		}

		if (UBRAnimInstance* Spine = Cast<UBRAnimInstance>(MeshComp->GetAnimInstance()))
		{
			Spine->SendSeamGameplayEvent(Tag);
		}
	}
}

UBRAnimNotifyState_GameplayEventWindow::UBRAnimNotifyState_GameplayEventWindow()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(220, 90, 40);
#endif
}

FString UBRAnimNotifyState_GameplayEventWindow::GetNotifyName_Implementation() const
{
	return BeginEventTag.IsValid()
		? BeginEventTag.ToString()
		: TEXT("BR Gameplay Event Window (NO TAG SET)");
}

void UBRAnimNotifyState_GameplayEventWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	SendThrough(MeshComp, BeginEventTag);
}

void UBRAnimNotifyState_GameplayEventWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// Runs on interruption too -- the montage terminating walks its active branching points in
	// reverse and ends each one. That is what makes the trace window cancel-clean without a
	// single line of cancellation code here (animation.md law 5).
	SendThrough(MeshComp, EndEventTag);
}
