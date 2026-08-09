// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSSprintState.generated.h"

/**
 * Sprint state effect:
 * - Infinite duration
 * - Grants IsSprinting state tag
 * - Writes MovementSpeed multiplier via SetByCaller (Data.Movement.SpeedMultiplier)
 *
 * Apply/Remove from GA_OSSprint (LocalPredicted).
 */
UCLASS()
class ONSIGHT_API UGE_OSSprintState : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSSprintState(const FObjectInitializer& ObjectInitializer);
};

