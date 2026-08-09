// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Dodge: Chooser picks direction, applies movement and State.Dodging. Block/cancel tags prevent activation when blocked.

#include "CoreMinimal.h"
#include "OSGameplayAbilityChooser.h"
#include "Data/OSDefenseAndReactions.h"
#include "GA_OSDodge.generated.h"

class UChooserTable;
/**
 *  
 */
UCLASS()
class ONSIGHT_API UGA_OSDodge : public UOSGameplayAbilityChooser
{
	GENERATED_BODY()

public:
	UGA_OSDodge();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:	
	UFUNCTION(BlueprintCallable)
	FOSDodgeInfoStruct EvaluateDodge();
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	virtual void ConnectTask(UAbilityTask* task) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	float DashImpulseStrength = 1200.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	float AirDashLateralFriction = 8.f;
	
	// Post-dodge braking deceleration override. Applied in EndAbility to give
	// dodge a snappy stop feel. Original CMC value is cached and restored.
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta=(ClampMin="0.0"))
	float PostDodgeBrakingDeceleration = 2048.f;

private:
	float CachedFallingLateralFriction = 0.f;
	float CachedBrakingDeceleration = 0.f;
	
};
