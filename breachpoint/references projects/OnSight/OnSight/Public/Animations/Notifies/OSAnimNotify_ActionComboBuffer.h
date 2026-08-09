// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSAnimNotifyState.h"
#include "OSAnimNotify_ActionComboBuffer.generated.h"

/**
 * NotifyState: enables combo input buffer window for its duration.
 * Adds Combo_CanQueue tag on NotifyBegin, removes it on NotifyEnd.
 * Fires Event_ComboWindowOpened on NotifyBegin so abilities can consume pre-buffered input immediately.
 *
 * Cancel windows (movement, dodge, block, jump) are handled separately via OSAnimNotifyState_CanBeCanceled
 * and the corresponding cancel checks in abilities/controller.
 */
UCLASS()
class ONSIGHT_API UOSAnimNotify_ActionComboBuffer : public UOSAnimNotifyState
{
	GENERATED_BODY()

public:
	UOSAnimNotify_ActionComboBuffer();
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
