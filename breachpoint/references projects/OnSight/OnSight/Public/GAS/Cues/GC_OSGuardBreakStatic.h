// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSGameplayCueNotifyStatic.h"
#include "GC_OSGuardBreakStatic.generated.h"

/** Static cue for guard break. Triggered when block is broken; plays VFX/audio. Chooser can pick variant. */
UCLASS()
class ONSIGHT_API UGC_OSGuardBreakStatic : public UOSGameplayCueNotifyStatic
{
	GENERATED_BODY()

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
