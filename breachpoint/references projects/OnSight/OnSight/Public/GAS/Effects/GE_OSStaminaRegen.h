// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSStaminaRegen.generated.h"

/**
 * Server-authoritative stamina regeneration (periodic).
 * Prefer a C++ base + optional BP child for tuning.
 */
UCLASS()
class ONSIGHT_API UGE_OSStaminaRegen : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSStaminaRegen(const FObjectInitializer& ObjectInitializer);
};

