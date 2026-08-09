#pragma once
// Combo melee attack ability (weapon-agnostic):
// - Montage sections represent hits (Hit_0, Hit_1, ...)
// - Phase-gated input buffering via gameplay events
// - Static motion-warp target injection ("AttackTarget") per hit
// - Server-only damage application on GameplayEvent.DirectDamage

#include "CoreMinimal.h"
#include "Data/OSCombatTypes.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_MeleeAttackCombo.generated.h"

class AOSCharacter;
class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

/**
 * GA_MeleeAttackCombo
 *
 * NOTE:
 * This ability inherits directly from UOSGameplayAbility (not GA_OSAttack).
 * As a result, OSAnimNotifyState_OSTrackMotionWarpTarget will NOT treat it as a GA-driven
 * attack ability for per-hit indexed warp naming or per-tick warp refresh. This GA uses
 * a single static warp target name (default "AttackTarget") injected before each hit.
 */
UCLASS()
class ONSIGHT_API UGA_MeleeAttackCombo : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MeleeAttackCombo();

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

	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** Combo-chain — Hit N's warp target must survive Hit (N-1)'s blend-out. */
	virtual bool PersistsWarpTargetsAcrossNotifyEnd() const override { return true; }

protected:
	// ========================================
	// COMBO MONTAGE
	// ========================================

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo")
	TObjectPtr<UAnimMontage> ComboMontage;

	/** If empty, defaults to montage section order (0..NumSections-1). */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo")
	TArray<FName> ComboSectionNames;

	/** If true: pressing during Recovery restarts from hit 0. If false: advances. */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo")
	bool bRecoveryPressRestartsCombo = true;

	/**
	 * If true, hits after the first one will spend BaseCosts again (via CheckCost/ApplyCost).
	 * The first hit cost is already handled by UOSGameplayAbility::ActivateAbility (CommitAbility).
	 */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo")
	bool bSpendCostsPerHit = false;

	// ========================================
	// INPUT TYPE (LIGHT/HEAVY) + CROSS-TYPE SWITCHING
	// ========================================

	/**
	 * The gameplay tag the controller passes into HandleComboInput() for this ability type.
	 *
	 * Examples in this project:
	 * - Light: Attack_Light
	 * - Heavy: Attack_Heavy
	 *
	 * If set, this GA supports cross-type buffering/switching:
	 * - Buffering during Windup/Active will switch on Recovery (end self + activate requested tag)
	 * - Pressing during Recovery with a different type will immediately end (controller can activate new type)
	 */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|Input")
	FGameplayTag InputActivationTag;

	// ========================================
	// MOTION WARPING (STATIC INJECTION)
	// ========================================

	/** Must match the AnimNotifyState warp target name. */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|MotionWarp")
	FName WarpTargetName = "AttackTarget";

	/** Standoff from target (cm). Floored by capsule stop distance. */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|MotionWarp", meta=(ClampMin="0", UIMin="0"))
	float OptimalMeleeDistance = 45.f;

	/** When no target exists, warp forward by this distance (cm). 0 = warp in place. */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|MotionWarp", meta=(ClampMin="0", UIMin="0"))
	float DirectionalFallbackWarpDistance = 50.f;

	// ========================================
	// SOFT TARGETING (OPTIONAL)
	// ========================================

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget")
	bool bSoftTargeting = true;

	/** If true and tracking is active, prefer the avatar's replicated SoftLockTarget. */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget|SoftLock")
	bool bPreferReplicatedSoftLockTarget = true;

	/** If true, once a target is acquired it will be reused for later hits if still valid/in range. */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget|SoftLock")
	bool bPersistTargetAcrossChain = true;

	/** If > 0, overrides the max range used for persistence checks. */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget|SoftLock", meta=(ClampMin="0", UIMin="0"))
	float PersistMaxRangeOverride = 0.f;

	/** Tracking state tag granted by GE_OSTracking (toggled by GA_OSSoftLock). */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget|SoftLock")
	FGameplayTag TrackingStateTag;

	/** If true, sends GameplayEvent.UI.TargetChanged when the resolved target changes. */
	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget|SoftLock")
	bool bBroadcastTargetChangedToUI = true;

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget", meta=(ClampMin="10", ClampMax="360", UIMin="10", UIMax="360"))
	float SoftTargetConeAngle = 90.f;

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget", meta=(ClampMin="50", UIMin="50"))
	float SoftTargetMaxRange = 500.f;

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget")
	FGameplayTagContainer SoftTargetExcludeTags;

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget")
	EOSBlendAlgorithm BlendAlgorithm = EOSBlendAlgorithm::Magnetic;

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget", meta=(ClampMin="0", ClampMax="1", UIMin="0", UIMax="1"))
	float InputInfluence = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget", meta=(ClampMin="0", ClampMax="1", UIMin="0", UIMax="1"))
	float MagneticStickiness = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|SoftTarget", meta=(ClampMin="5", ClampMax="180", UIMin="5", UIMax="180"))
	float MagneticBreakawayAngle = 60.f;

	// ========================================
	// DAMAGE (SERVER-ONLY)
	// ========================================

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|Damage", meta=(ClampMin="0", UIMin="0"))
	float BaseDamage = 25.f;

	UPROPERTY(EditDefaultsOnly, Category="OnSight|MeleeCombo|Damage")
	EOSAttackType AttackType = EOSAttackType::Light;

private:
	enum class EMeleeComboPhase : uint8
	{
		None,
		Active,
		Recovery
	};

	int32 CurrentHitIndex = 0;
	bool bPendingComboInput = false;
	bool bComboBufferedTagSet = false;
	EMeleeComboPhase Phase = EMeleeComboPhase::None;
	FGameplayTag BufferedCrossTypeTag;
	bool bAddedIsAttackingLooseTag = false;

	TWeakObjectPtr<AOSCharacter> CurrentTarget;
	TWeakObjectPtr<AOSCharacter> PersistedTarget;
	FVector StoredBlendedDirection2D = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitComboInputTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitPhaseActiveTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitPhaseRecoveryTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitDirectDamageTask;

	void StartPersistentEventListeners();
	void CleanupTasks();

	void StartHit(int32 HitIndex);
	FName GetSectionNameForHit(int32 HitIndex) const;

	void ResolveSoftTarget_WithSoftLock(AOSCharacter* OwnerChar);
	float GetPersistMaxRange() const;
	bool IsValidTargetForPersist(AOSCharacter* OwnerChar, AOSCharacter* Target, float MaxRange) const;
	void SetLockedOnState(bool bLockedOn);
	void InjectWarpTarget(AOSCharacter* OwnerChar);
	void ClearWarpTarget(AOSCharacter* OwnerChar);

	FGameplayTag GetEffectiveInputTag() const;

	/** Ends this ability and attempts to activate another ability by tag on the same ASC. */
	void SwitchToAbilityByTag(const FGameplayTag& SwitchToTag);

	// Event handlers
	UFUNCTION()
	void OnComboInputReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnPhaseActiveReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnPhaseRecoveryReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnDirectDamageReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnHitMontageEnded();
};

