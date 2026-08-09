// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSAnimNotify.h"
#include "OSAnimNotify_CancelActionBuffer.generated.h"

// TO BE DELETED


/** Notify: cancels attack abilities and/or clears combo buffer (e.g. recovery, interrupt). */
UCLASS()
class ONSIGHT_API UOSAnimNotify_CancelActionBuffer : public UOSAnimNotify
{
	GENERATED_BODY()

public:
	UOSAnimNotify_CancelActionBuffer();
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** If true, this notify only cancels attacks when the character is currently dodging/blocking (or falling, if enabled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cancel Action Buffer")
	bool bOnlyCancelIfInDodgeBlockJump = true;

	/** If true, treat MovementMode==Falling as "jumping" for the purposes of bOnlyCancelIfInDodgeBlockJump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cancel Action Buffer", meta=(EditCondition="bOnlyCancelIfInDodgeBlockJump"))
	bool bTreatFallingAsJumping = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cancel Action Buffer")
	bool bCancelAllAttacks = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cancel Action Buffer")
	bool bClearComboInput = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cancel Action Buffer")
	bool bDisableComboBuffer = true;
};
