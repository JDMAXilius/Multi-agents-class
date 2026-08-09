// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSAuraRegen.generated.h"

/*
Server-authoritative aura regeneration (periodic).
Prefer a C++ base + optional BP child for tuning.
*/
UCLASS()
class ONSIGHT_API UGE_OSAuraRegen : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSAuraRegen(const FObjectInitializer& ObjectInitializer);
};

