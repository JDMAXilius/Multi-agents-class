// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OSAnimNotify.h"
#include "OSAnimNotifyGameplayCue.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSAnimNotifyGameplayCue : public UOSAnimNotify
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="GameplayCue")
	FGameplayTag CueTag;
	
	UPROPERTY(EditAnywhere, Category="GameplayCue")
	FName SocketName = NAME_None;

protected:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	

};
