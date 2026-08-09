// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSDowned.generated.h"

/**
 * Infinite "downed" state effect applied to a grab victim on entering the Proned section of
 * GA_OSGrabReaction. Removed on Getup end (alive path) or pawn destruction (dead path).
 *
 * Grants Gameplay.State.IsDowned (semantic state tag — consumed by anim, UI, AI)
 * and Gameplay.State.Invincibility (damage pipeline already respects this tag as an exclude).
 * Downed implies invulnerable by design.
 */
UCLASS()
class ONSIGHT_API UGE_OSDowned : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSDowned(const FObjectInitializer& ObjectInitializer);
};
