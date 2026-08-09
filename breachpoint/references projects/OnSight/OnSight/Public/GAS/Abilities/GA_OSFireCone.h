// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "Data/OSCombatTypes.h"
#include "GA_OSFireCone.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class AOSCharacter;

/**
 *  Forward cone-shaped fire attack.
 *  Performs a sphere overlap + angle check to damage all valid targets in a cone.
 *  Montage-driven: a gameplay event (GameplayEvent.DirectDamage) from an anim notify
 *  triggers the cone damage sweep. Optionally ticks damage over a sustained duration.
 */
UCLASS()
class ONSIGHT_API UGA_OSFireCone : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSFireCone();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// ========== Cone Configuration ==========

	/** Half-angle of the cone in degrees (e.g., 45 = 90 degree total cone) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone", meta = (ClampMin = 5, ClampMax = 90))
	float ConeHalfAngleDegrees = 30.0f;

	/** Max range of the cone from the caster */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone", meta = (ClampMin = 50))
	float ConeRange = 600.0f;

	/** Damage per hit (or per tick if sustained) */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone", meta = (ClampMin = 0))
	//float Damage = 25.0f;

	/** Attack type for the damage context */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone")
	EOSAttackType AttackType = EOSAttackType::Special;

	// ========== Sustained Fire (optional) ==========

	/** If true, the cone deals damage repeatedly over SustainDuration */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone|Sustained")
	//bool bSustainedFire = false;
	///** How long sustained fire lasts (seconds) */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone|Sustained", meta = (EditCondition = "bSustainedFire", ClampMin = 0.1))
	//float SustainDuration = 2.0f;
	///** Interval between sustained damage ticks */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone|Sustained", meta = (EditCondition = "bSustainedFire", ClampMin = 0.1))
	//float SustainTickInterval = 0.25f;

	// ========== Animation ==========

	/** Cast montage (should have an AN_OSDirectDamage notify to trigger the cone) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone|Animation")
	TObjectPtr<UAnimMontage> FireMontage;

	// ========== Damage Effect ==========

	/** GE class to apply (defaults to GE_OSApplyDamage) */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone|Damage")
	//TSubclassOf<UGameplayEffect> DamageEffectClass;

	// ========== FX ==========

	/** Gameplay tag for the FX event (fired via FX subsystem or gameplay cue) */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone|FX")
	//FGameplayTag FireFXTag;
	
	// ========== Debug ==========

	/** Draw debug cone visualization when ability fires */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone|Debug")
	bool bDrawDebugCone = false;

	/** How long the debug cone stays visible (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone|Debug", meta = (EditCondition = "bDrawDebugCone", ClampMin = 0.1))
	float DebugDrawDuration = 2.0f;

	// ========== Tags to exclude from targeting ==========

	/** Targets with any of these tags are immune */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Cone")
	FGameplayTagContainer ExcludeTargetTags;

protected:

	/** Perform the cone damage sweep — called by event or timer */
	void PerformConeDamage();

	/** Gather all valid AOSCharacter targets in the cone */
	TArray<AOSCharacter*> GatherTargetsInCone() const;

	/** Check if an actor is within the cone angle */
	bool IsInCone(const FVector& Origin, const FVector& Forward, const AActor* Target) const;

	/** Draw debug visualization of the cone */
	void DrawDebugConeVisualization(const FVector& Origin, const FVector& Forward, const TArray<AOSCharacter*>& HitTargets) const;

private:

	/** Called when the montage's DirectDamage event fires */
	UFUNCTION()
	void OnDamageEventReceived(FGameplayEventData Payload);

	/** Called when the montage completes or is interrupted */
	UFUNCTION()
	void OnMontageFinished();

	/** Sustained fire tick */
	//void OnSustainTick();

	//FTimerHandle SustainTimerHandle;
	//int32 SustainTicksRemaining = 0;
	//bool bFireCueActive = false;

	/** Track already-hit actors for single-fire mode to prevent double hits */
	UPROPERTY()
	TSet<TObjectPtr<AActor>> HitActorsThisCast;
};
