#pragma once
// Combo melee attack: montage sections as hits, input buffering, cross-type switching.
// Uses GA_OSAttack for soft targeting + motion warp injection.

#include "CoreMinimal.h"
#include "Data/OSCombatTypes.h"
#include "GAS/Abilities/GA_OSAttack.h"
#include "GameplayTagContainer.h"
#include "GA_MeleeAttackCombonext.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

/**
 * GA_MeleeAttackCombonext
 *
 * Combo flow: montage-driven.
 *   - Input is buffered when received during a hit.
 *   - OnHitMontageEnded advances or ends based on buffered input.
 *   - No phase notifies required — each section plays to completion.
 */
UCLASS()
class ONSIGHT_API UGA_MeleeAttackCombonext : public UGA_OSAttack
{
	GENERATED_BODY()

public:
	UGA_MeleeAttackCombonext();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual void OnRemoveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;

protected:
	// -----------------------------------------------------------------------
	// COMBO MONTAGE
	// -----------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "OnSight|MeleeCombo")
	TObjectPtr<UAnimMontage> ComboMontage;

	/** If empty, falls back to montage section order (0..NumSections-1). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|MeleeCombo")
	TArray<FName> ComboSectionNames;

	/** True: hits after the first spend BaseCosts again (CheckCost/ApplyCost). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|MeleeCombo")
	bool bSpendCostsPerHit = false;

	// -----------------------------------------------------------------------
	// INPUT TYPE + CROSS-TYPE SWITCHING
	// -----------------------------------------------------------------------

	/** Tag this ability listens for (e.g. Attack_Light, Attack_Heavy).
	 *  Enables cross-type buffering: opposite type buffered during a hit
	 *  ends this ability and activates the requested one on montage end. */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|MeleeCombo|Input")
	FGameplayTag InputActivationTag;

	// -----------------------------------------------------------------------
	// DAMAGE
	// -----------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "OnSight|MeleeCombo|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "OnSight|MeleeCombo|Damage", meta = (ClampMin = "0", UIMin = "0"))
	float BaseDamage = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "OnSight|MeleeCombo|Damage")
	EOSAttackType AttackType = EOSAttackType::Light;

private:
	// -----------------------------------------------------------------------
	// COMBO STATE
	// -----------------------------------------------------------------------

	int32 CurrentHitIndex          = 0;
	bool bPendingComboInput        = false;
	bool bComboBufferedTagSet      = false;
	FGameplayTag BufferedCrossTypeTag;
	bool bAddedIsAttackingLooseTag = false;

public:
	/** Unique per-hit warp name: "AttackTarget_<CurrentHitIndex>" */
	virtual int32 GetWarpHitIndex() const override { return CurrentHitIndex; }

	// -----------------------------------------------------------------------
	// TASKS
	// -----------------------------------------------------------------------

	UPROPERTY() TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;
	UPROPERTY() TObjectPtr<UAbilityTask_WaitGameplayEvent>  WaitComboInputTask;
	UPROPERTY() TObjectPtr<UAbilityTask_WaitGameplayEvent>  WaitDirectDamageTask;

	// -----------------------------------------------------------------------
	// INTERNAL METHODS
	// -----------------------------------------------------------------------

	void StartComboListeners();
	void CleanupTasks();

	void StartHit(int32 HitIndex);
	FName GetSectionNameForHit(int32 HitIndex) const;

	FGameplayTag GetEffectiveInputTag() const;
	void SwitchToAbilityByTag(const FGameplayTag& SwitchToTag);

	UFUNCTION() void OnComboInputReceived(FGameplayEventData Payload);
	UFUNCTION() void OnDirectDamageReceived(FGameplayEventData Payload);
	UFUNCTION() void OnHitMontageEnded();
};

