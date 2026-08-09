#pragma once
// Combo attack: phase-gated input buffering, montage sections, type-switching (light/heavy).

#include "CoreMinimal.h"
#include "GAS/Abilities/GA_OSAttack.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GA_OSComboAttack.generated.h"

class AActor;
class ACharacter;
class UGameplayEffect;

/** Multi-hit combo ability. Phase-gated input buffering: Windup/Active buffer, Recovery resolves.
 *  Uses ANS_OSAttackPhase notify for phase transitions. Input via GameplayEvent.ComboInputPressed. */
UCLASS()
class ONSIGHT_API UGA_OSComboAttack : public UGA_OSAttack
{
	GENERATED_BODY()

public:
	UGA_OSComboAttack();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ComboAttack")
	TObjectPtr<UAnimMontage> ComboMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ComboAttack")
	TArray<FName> ComboSectionNames;

	/** Attack type identity for combo type-switching (light vs heavy).
	 *  Defaults to Attack_Light; heavy BP subclass overrides to Attack_Heavy.
	 *  Used by TryCombo to detect type changes and reset the combo chain. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OnSight|ComboAttack")
	FGameplayTag AttackTypeTag;

	UFUNCTION() void OnMontageSectionEnded();
	UFUNCTION() void OnComboInputReceived(FGameplayEventData Payload);
	void             PerformComboHit( int32 ComboIndex );

	void         TryAttack(const FGameplayTag& AttackType);
	void         TryCombo(const FGameplayTag& AttackType);
	bool         IsPhaseValid() const;
	virtual void OnPhaseChanged(EOSAttackPhase OldPhase, EOSAttackPhase NewPhase) override;

public:
	/** Warp hit index: the index of the ACTIVE hit (set before montage plays).
	 *  CurrentComboIndex is already incremented to the NEXT hit by TryCombo,
	 *  so we store the active one separately to avoid off-by-one in notify ticks. */
	virtual int32 GetWarpHitIndex() const override { return ActiveHitIndex; }

	UFUNCTION(BlueprintCallable, Category = "OnSight|ComboAttack")
	int32 GetCurrentComboIndex() const { return CurrentComboIndex; }
	// Attempts to apply cost for the current combo hit (outside the first hit, which was already checked by ability activation)
	bool ApplyCostPerHit();

private:
	/** Shared teardown: ends tasks, clears combo buffer tag, resets state.
	 *  Called by both EndAbility and OnRemoveAbility. IsAttacking tag is managed by
	 *  ActivationOwnedTags (auto-granted on activate, auto-removed on end) — not here. */
	void CleanupComboState();
	void CleanupAbilityTasks();
	void GetEffectiveSectionNames(TArray<FName>& OutNames) const;

	/** Two-level combo buffer:
	 *  - bPendingComboInput: ability-local, true when input received during Windup/Active,
	 *    consumed (set false) by OnPhaseChanged when it resolves the buffer.
	 *  - State_Attack_ComboBuffered: replicated loose tag (Mixed ASC), set on first buffer for the chain.
	 *    Read by AOSPlayerController::OnMoveStarted to skip attack cancel when buffered.
	 *    Cleared only in CleanupComboState (EndAbility / OnRemoveAbility). */
	void SetComboBuffered(bool bBuffered);
	bool HasPendingComboInput() const { return bPendingComboInput; }
	bool bPendingComboInput = false;

	UPROPERTY()
	FGameplayTag PreviousAttackType;

	/** For cross-type buffers: the requested type's activation tag (e.g. Attack_Light).
	 *  Valid only when HasComboBuffer() is true. Invalid = same-type buffer. */
	UPROPERTY()
	FGameplayTag BufferedCrossTypeTag;
	int32 CurrentComboIndex = 0;
	/** The combo index of the currently PLAYING hit. Set in PerformComboHit before montage starts.
	 *  Distinct from CurrentComboIndex which is already incremented to the NEXT hit. */
	int32 ActiveHitIndex = 0;

	UPROPERTY() TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveWaitInputTask;
	UPROPERTY() TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

	TArray<FName> EffectiveSectionNames;
};
