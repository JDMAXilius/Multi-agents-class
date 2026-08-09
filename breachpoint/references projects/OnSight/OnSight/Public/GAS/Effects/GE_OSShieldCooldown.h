// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSShieldCooldown.generated.h"

/**
 * Base class for the shield cooldown gameplay effect.
 * Create a Blueprint child (recommended asset name: GE_Shield_Cooldown) and configure:
 * - Duration (ex: 5s)
 * - Granted tag: Cooldown.Shield
 */
UCLASS()
class ONSIGHT_API UGE_OSShieldCooldown : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSShieldCooldown(const FObjectInitializer& ObjectInitializer);
};

