// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
/* Data-driven cone damage magic ability.
   Inherits GA_OSBaseMagic for montage wiring + VFX cue slots.
   Configure cone parameters, damage effects, and VFX in Blueprint class defaults: no C++ per grimoire. */

#include "CoreMinimal.h"
#include "GAS/Abilities/GA_OSBaseMagic.h"
#include "GA_OSMagicCone.generated.h"

enum class EOSAttackType : uint8;
class AOSCharacter;

UCLASS(Abstract)
class ONSIGHT_API UGA_OSMagicCone : public UGA_OSBaseMagic
{
	GENERATED_BODY()

public:
	UGA_OSMagicCone();

	// --- Cone Configuration ---

	/* Half-angle of the damage cone in degrees (e.g., 45 = 90 degree total cone). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cone", meta = (ClampMin = 5, ClampMax = 90))
	float ConeHalfAngleDegrees;

	/* Max range of the cone from the caster. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cone", meta = (ClampMin = 50))
	float ConeRange;

	/* Attack type for the damage context. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cone")
	EOSAttackType AttackType;

	/* Targets with any of these tags are immune. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cone")
	FGameplayTagContainer ExcludeTargetTags;

	// --- Debug ---

	/* Draw debug cone visualization when ability fires. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cone|Debug")
	bool bDrawDebugCone;

	/* How long the debug cone stays visible (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cone|Debug", meta = (EditCondition = "bDrawDebugCone", ClampMin = 0.1))
	float DebugDrawDuration;

protected:
	virtual void OnMagicEventReceived_Implementation(FGameplayEventData Payload) override;

private:
	void PerformConeDamage();
	TArray<AOSCharacter*> GatherTargetsInCone() const;
	bool IsInCone(const FVector& Origin, const FVector& Forward, const AActor* Target) const;
	void DrawDebugConeVisualization(const FVector& Origin, const FVector& Forward, const TArray<AOSCharacter*>& HitTargets) const;

	UPROPERTY()
	TSet<TObjectPtr<AActor>> HitActorsThisCast;
};
