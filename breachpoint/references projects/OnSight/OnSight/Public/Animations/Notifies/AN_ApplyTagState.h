// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OSAnimNotifyState.h"
#include "AN_ApplyTagState.generated.h"

/** Notify state: add StateTag to avatar ASC on Begin, remove on End. Use for temporary state (e.g. attacking window). */
UCLASS()
class ONSIGHT_API UAN_ApplyTagState : public UOSAnimNotifyState
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
	FGameplayTag StateTag;
	
protected:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
	void ApplyState( AActor* owner, const bool toAdd ) const;
};
