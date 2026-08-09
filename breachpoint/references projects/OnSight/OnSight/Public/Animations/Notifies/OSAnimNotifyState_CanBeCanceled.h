#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "OSAnimNotifyState_CanBeCanceled.generated.h"

/**
 * NotifyState: grants "cancel permission" tags for its duration.
 * Use this on montages to define when the current attack can be canceled into dodge/block/jump.
 *
 * Example tags:
 * - Gameplay.State.CanCancel.Dodge
 * - Gameplay.State.CanCancel.Block
 * - Gameplay.State.CanCancel.Jump
 */

// TO BE DELETED

UCLASS()
class ONSIGHT_API UOSAnimNotifyState_CanBeCanceled : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cancel")
	FGameplayTagContainer CancelPermissionTags;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};

