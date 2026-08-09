// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OSAnimNotifyState.h"
#include "AN_SendGameplayEventState.generated.h"

/**
 * AnimNotifyState version of AN_SendGameplayEvent. Sends an event under a specific tag at begin and end.
 * Intended for GAS Abilities that start and end at certain points in the animation.
 */
UCLASS(meta=(DisplayName="AN_SendGameplayEventState"))
class ONSIGHT_API UAN_SendGameplayEventState : public UOSAnimNotifyState
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
	FGameplayTag OnBeginTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
	FGameplayTag OnEndTag;
	
private:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
	void SendEventFromTag( AActor* owner, const FGameplayTag& tag ) const;
};
