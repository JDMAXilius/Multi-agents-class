// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "OSAnimNotifyState.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSAnimNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category="OS Anim Notify")
	FName CustomName = NAME_None;
	
protected:	
	virtual FString GetNotifyName_Implementation() const override;
	
	FName DefaultName = NAME_None;
};
