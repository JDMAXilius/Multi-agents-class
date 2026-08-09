// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSGameplayCueNotifyStatic.h"
#include "GC_OSFlinchStatic.generated.h"

/** Static cue for flinch (light hit reaction). Triggered by GameplayCue.Flinch tag; plays VFX/audio. */
UCLASS()
class ONSIGHT_API UGC_OSFlinchStatic : public UOSGameplayCueNotifyStatic
{
	GENERATED_BODY()
	
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
