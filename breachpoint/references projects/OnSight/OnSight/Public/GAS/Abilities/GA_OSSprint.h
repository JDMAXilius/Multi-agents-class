// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Toggle sprint: adds/removes sprint GE (movement speed), stamina drain while active. Blocked by dead/hit-react tags.

#include "CoreMinimal.h"
#include "OSGameplayAbility.h"
#include "GA_OSSprint.generated.h"

class UGameplayEffect;

/**
 * Sprint ability that increases movement speed while draining stamina.
 * Grants IsSprinting state tag via GameplayEffect for animation synchronization.
 */
UCLASS()
class ONSIGHT_API UGA_OSSprint : public UOSGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_OSSprint();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, 
		const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, 
		bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayTagContainer* SourceTags = nullptr, 
		const FGameplayTagContainer* TargetTags = nullptr, 
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// ========================================
	// SPRINT CONFIGURATION
	// ========================================
	
	/** Movement speed multiplier when sprinting (e.g., 1.6 = 60% faster) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sprint", meta=(ClampMin="1.0", ClampMax="3.0"))
	float SprintSpeedMultiplier = 1.6f;

	/** Minimum velocity threshold to maintain sprint (squared for performance) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sprint")
	float MinVelocityThresholdSquared = 100.0f;

	/** Camera distance when sprinting (closer to player) */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sprint|Camera", meta=(ClampMin="0.0"))
	//float SprintCameraDistance = 250.0f;

	/** Default camera distance (restored when sprint ends) */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sprint|Camera", meta=(ClampMin="0.0"))
	//float DefaultCameraDistance = 400.0f;

	/** Camera interpolation speed when changing distance */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sprint|Camera", meta=(ClampMin="0.1", ClampMax="20.0"))
	//float CameraInterpSpeed = 5.0f;
	
	// ========================================
	// GAMEPLAY EFFECT HANDLES
	// ========================================
	
	/** Handle to the sprint state effect (grants IsSprinting tag + modifies speed) */
	FActiveGameplayEffectHandle SprintStateEffectHandle;

	/** Timer handle for camera distance interpolation */
	FTimerHandle CameraInterpTimerHandle;

	/** Sprint state GE (Infinite, grants IsSprinting, overrides MovementSpeed via SetByCaller) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sprint")
	TSubclassOf<UGameplayEffect> SprintStateEffectClass;

	// ========================================
	// HELPER FUNCTIONS
	// ========================================
	
	/** Apply GameplayEffect that grants IsSprinting state tag and modifies MovementSpeed */
	void ApplySprintStateEffect();

	/** Remove GameplayEffect that grants IsSprinting state tag and modifies MovementSpeed */
	void RemoveSprintStateEffect();

	/** Check if character is moving fast enough to maintain sprint */
	bool IsCharacterMoving() const;

	/** Check if character has enough stamina to continue sprinting */
	bool HasStaminaToSprint() const;

	/** Start the repeating check loop for movement and stamina */
	void StartSprintCheckLoop();

	/** Called periodically to check if sprint should continue */
	UFUNCTION()
	void OnSprintCheck();

	/** Update camera distance (GAS best practice: ability controls when, character handles how) */
	//void UpdateCameraDistance(float TargetDistance);

	// ========================================
	// TAG-DRIVEN CANCEL
	// ========================================

	/** Registered on ASC in ActivateAbility, unregistered in EndAbility.
	 *  Cancels sprint the instant IsInAir tag appears (e.g. jump, fall off ledge).
	 *  Same reactive delegate pattern as GA_OSBlock's guard break detection. */
	void OnInAirTagChanged(const FGameplayTag Tag, int32 NewCount);

	FDelegateHandle InAirTagDelegateHandle;
};

