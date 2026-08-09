// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Sustained block: applies block GE, converts incoming damage to stamina drain. Guard break when stamina breaks (AttributeSet fires event).

#include "CoreMinimal.h"
#include "OSGameplayAbilityChooser.h"
#include "GA_OSBlock.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnActiveGameplayEffectAddedDelegateToSelf,
	UAbilitySystemComponent*,               
	const FGameplayEffectSpec&,             
	FActiveGameplayEffectHandle);           

struct FOSBlockInfo;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UGA_OSBlock : public UOSGameplayAbilityChooser
{
	GENERATED_BODY()
	
public:
	UGA_OSBlock();
	
	FOSBlockInfo EvaluateBlock();
	
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:

	/** Block state GE (Infinite, grants IsBlock, overrides MovementSpeed via SetByCaller) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block")
	TSubclassOf<UGameplayEffect> BlockStateEffectClass;

	/** Handle to the Block state effect (grants IsBlock tag + modifies speed) */
	FActiveGameplayEffectHandle BlockStateEffectHandle;

	/** Movement speed multiplier when blocking (e.g., 0.6 = 60% slower) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlockSpeedMultiplier = 0.5f;

private:
	
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag IsBlocking;
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag IsBroken;
	
	UPROPERTY(EditDefaultsOnly, Category="Guard")
	TSubclassOf<UGameplayEffect> GuardBreakEffectClass;

	FDelegateHandle GuardBreakDelegateHandle;
	
	UFUNCTION()
	void OnEffectAppliedToSelf(UAbilitySystemComponent* TargetASC,
							   const FGameplayEffectSpec& SpecApplied,
							   FActiveGameplayEffectHandle ActiveHandle);

	/** Apply GameplayEffect that grants IsBlocking state tag and modifies MovementSpeed */
	void ApplyBlockStateEffect();

	/** Remove GameplayEffect that grants IsBlocking state tag and modifies MovementSpeed */
	void RemoveBlockStateEffect();


};
