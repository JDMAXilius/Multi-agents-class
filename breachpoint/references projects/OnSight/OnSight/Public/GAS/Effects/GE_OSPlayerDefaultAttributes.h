// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSPlayerDefaultAttributes.generated.h"

/**
 * C++ "pure GAS" default attribute initializer.
 * Use this (or a BP child of it) as your Character/PlayerState startup attribute GE.
 *
 * Why:
 * - Avoids BP GE misconfiguration (Attribute=None modifier warnings).
 * - Keeps authoritative attribute initialization in a single, deterministic GE.
 */
UCLASS()
class ONSIGHT_API UGE_OSPlayerDefaultAttributes : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSPlayerDefaultAttributes();
};

