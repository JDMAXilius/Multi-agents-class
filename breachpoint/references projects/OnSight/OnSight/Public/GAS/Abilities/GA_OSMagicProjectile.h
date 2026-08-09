// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
/* Data-driven projectile magic ability.
   Inherits GA_OSBaseMagic for montage wiring + VFX cue slots.
   Configure projectile class, spawn offset, and VFX in Blueprint class defaults: no C++ per grimoire. */

#include "CoreMinimal.h"
#include "GAS/Abilities/GA_OSBaseMagic.h"
#include "GA_OSMagicProjectile.generated.h"

class AOSProjectile;

UCLASS(Abstract)
class ONSIGHT_API UGA_OSMagicProjectile : public UGA_OSBaseMagic
{
	GENERATED_BODY()

public:
	UGA_OSMagicProjectile();

	// --- Projectile Configuration ---

	/* Projectile Blueprint class to spawn. The projectile BP owns speed, lifetime, VFX, collision, and payload.
	   Spawn location and direction come from base magic's OriginSocket / OriginForwardOffset / bAddOffsetToSocket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<AOSProjectile> ProjectileClass;

protected:
	virtual void OnMagicEventReceived_Implementation(FGameplayEventData Payload) override;

private:
	void SpawnProjectile();
};
