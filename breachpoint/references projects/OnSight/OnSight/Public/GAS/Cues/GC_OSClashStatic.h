// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSGameplayCueNotifyStatic.h"
#include "GC_OSClashStatic.generated.h"

/** Static cue for weapon clash. Uses effect context (FOSGameplayEffectContext) for clash info. OnActive plays VFX/audio. */
UCLASS()
class ONSIGHT_API UGC_OSClashStatic : public UOSGameplayCueNotifyStatic
{
	GENERATED_BODY()
	
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
public:
	int32 HitIndex = 1;
};
