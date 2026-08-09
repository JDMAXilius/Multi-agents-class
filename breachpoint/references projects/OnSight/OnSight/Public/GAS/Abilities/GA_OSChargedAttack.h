#pragma once
// Charged attack: commit window, then applies damage GE and sends Event_AttackHit to target. Uses GAS attack trace or similar for target.

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GA_OSChargedAttack.generated.h"

// DEPRECATED: Replaced by GA_OSComboAttack heavy path. Constructor uses loose
// FGameplayTag::RequestGameplayTag calls (bErrorIfNotFound=false) because CDO
// creation can run before the tag manager is ready. Do not extend: remove once
// all Blueprint references are migrated.
UCLASS()
class ONSIGHT_API UGA_OSChargedAttack : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSChargedAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	/** Motion warping target name (must match montage notify). Used by ClearAlignmentState on EndAbility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack")
	FName WarpTargetName = "AttackTarget";

	/** Base damage (scales with charge time) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack")
	float BaseDamage = 2.0f;

	/** Maximum damage at full charge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack")
	float MaxDamage = 5.0f;

	/** Time to reach full charge (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack", meta = (ClampMin = 0.1, ClampMax = 5.0))
	float MaxChargeTime = 2.0f;

	/** Minimum charge time before attack can be released (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack", meta = (ClampMin = 0.0, ClampMax = 2.0))
	float MinChargeTime = 0.5f;

	/** Stamina cost per second while charging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack")
	float StaminaCostPerSecond = 10.0f;

	/** Charged attack animation montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack")
	TObjectPtr<UAnimMontage> ChargedAttackMontage;

	/** Name of montage section for charge loop */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack")
	FName ChargeLoopSection = NAME_None;

	/** Name of montage section for attack release */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack")
	FName ChargeAttackSection = NAME_None;

	/** GameplayEffect class to apply damage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** GameplayEffect class to apply stamina cost */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ChargedAttack")
	TSubclassOf<UGameplayEffect> StaminaCostEffectClass;

	/** Blueprint event for charged attack */
	UFUNCTION(BlueprintImplementableEvent, Category = "OnSight|ChargedAttack")
	void OnChargedAttackPerformed(float ChargeAmount);

	/** Performs the charged attack with calculated damage */
	void PerformChargedAttack(float ChargeAmount);
	
	/** Performs attack trace (BlueprintCallable for anim notify usage) */
	UFUNCTION(BlueprintCallable, Category = "OnSight|ChargedAttack")
	void PerformAttackTrace();

	/** Called when charge loop montage completes */
	UFUNCTION()
	void OnChargeLoopCompleted();

	/** Called when charged attack montage completes */
	UFUNCTION()
	void OnChargedAttackCompleted();

	/** Called when charged attack is cancelled */
	UFUNCTION()
	void OnChargedAttackCancelled();

private:
	float ChargeStartTime = 0.0f;
	bool bIsCharging = false;
	bool bHasReleasedInput = false;
	FTimerHandle ChargeCostTimerHandle;
	float StoredChargeDamage = 0.0f;
};
