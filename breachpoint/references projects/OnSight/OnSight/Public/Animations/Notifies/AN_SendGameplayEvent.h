// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OSAnimNotify.h"
#include "AN_SendGameplayEvent.generated.h"

/**
 * AnimNotify that fires a GameplayEvent to the montage owner.
 * Intended for GAS abilities waiting on UAbilityTask_WaitGameplayEvent.
 */
UCLASS(meta=(DisplayName="AN_SendGameplayEvent"))
class ONSIGHT_API UAN_SendGameplayEvent : public UOSAnimNotify
{
	GENERATED_BODY()

public:
	/** Tag to send (ex: GameplayEvent.Montage.Trigger) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
	FGameplayTag EventTag;

	/** Optional scalar payload (0 if unused) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
	float EventMagnitude = 0.f;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

