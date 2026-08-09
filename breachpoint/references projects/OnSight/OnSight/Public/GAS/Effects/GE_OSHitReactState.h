// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSHitReactState.generated.h"

/**
 * Grants Gameplay.State.IsHitReacting for a short duration.
 */
UCLASS()
class ONSIGHT_API UGE_OSHitReactState : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSHitReactState(const FObjectInitializer& ObjectInitializer);
};

