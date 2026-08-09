// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSGameplayAbility.h"
#include "GA_OSJump.generated.h"

/**
 * Instant locomotion: commit via UOSGameplayAbility, surface trace for Wwise, ACharacter::Jump().
 * Asset tag Gameplay.Ability.Jump; LocalPredicted. CancelAbilitiesWithTag clears Attack / ChargedAttack / Grab on activate (jump-cancel).
 */
UCLASS()
class ONSIGHT_API UGA_OSJump : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSJump();

	/** Line trace end offset from capsule location (typically negative Z) for jump takeoff surface / Wwise switch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump")
	FVector LandTraceOffset = FVector(0.f, 0.f, -320.f);
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayTagContainer* SourceTags = nullptr, 
		const FGameplayTagContainer* TargetTags = nullptr, 
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	
	// Wwise and its various events are set in blueprint, making it difficult to properly implement in backend without hardcoding or overcomplicating the ability.
	// This hook aims to alleviate that by allowing the blueprint to dictate what material makes what sound.
	UFUNCTION(BlueprintImplementableEvent)
	void BPEvent_SetWWiseSwitchBasedOnSurface(const AActor* Actor, const EPhysicalSurface Surface) const;
	
	UFUNCTION(BlueprintImplementableEvent)
	void BPEvent_PostWwiseEvent(const AActor* Actor, bool bIsLocallyControlled) const;
};
