// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSDeathState.generated.h"

/**
 * Infinite "dead" state effect: grants Gameplay.State.IsDead and fires optional death cue.
 */
UCLASS()
class ONSIGHT_API UGE_OSDeathState : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSDeathState(const FObjectInitializer& ObjectInitializer);
};

