// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSGiveShield.generated.h"

/**
 * Base class for the "Give Shield" gameplay effect.
 * Create a Blueprint child (recommended asset name: GE_GiveShield) and configure:
 * - Duration (ex: 15s)
 * - Granted tag: Status.Buff.Shield
 * - GameplayCue: GameplayCue.ShieldBubble
 */
UCLASS()
class ONSIGHT_API UGE_OSGiveShield : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSGiveShield(const FObjectInitializer& ObjectInitializer);
};

