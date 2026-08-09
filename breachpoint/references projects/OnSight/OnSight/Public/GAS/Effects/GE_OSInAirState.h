// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSInAirState.generated.h"

/**
 * Infinite state effect: grants Gameplay.State.InAir while the character is airborne.
 *
 * Lifecycle:
 *   - Applied by AOSCharacter::OnMovementModeChanged when entering Falling/Flying (server-only).
 *   - Removed by AOSCharacter::OnMovementModeChanged when entering Walking/NavWalking.
 *   - Cleaned up by AOSCharacter::ClearDeathStateForRespawn on respawn.
 *
 * Replication:
 *   GAS replicates the GE automatically (Mixed mode). Clients receive the IsInAir tag
 *   through normal GE replication — no loose tags, no ROLE_SimulatedProxy gating needed.
 *   This matches the pattern used by GE_OSSprintState (IsSprinting) and GE_OSDeathState (IsDead).
 *
 * Design:
 *   IsGrounded is the implicit default (absence of this effect). OSAnimInstance derives
 *   GameplayTag_IsGrounded as !GameplayTag_IsInAir. The Gameplay.State.IsGrounded tag
 *   declaration is retained in DefaultGameplayTags.ini for Blueprint compatibility.
 */
UCLASS()
class ONSIGHT_API UGE_OSInAirState : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSInAirState(const FObjectInitializer& ObjectInitializer);
};
