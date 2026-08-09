#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GA_OSGrabReaction.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UGameplayEffect;

/**
 * Victim-side grab reaction: triggered by GameplayEvent.GrabHit from attacker's GA_OSGrab.
 * Receives victim montage from attacker via FGameplayEventData.OptionalObject (paired set).
 * Thin state shell: manages IsGrabbed tag lifecycle via ActivationOwnedTags.
 */
UCLASS()
class ONSIGHT_API UGA_OSGrabReaction : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSGrabReaction();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// Fallback montage if attacker doesn't provide one via GameplayEvent.OptionalObject
	UPROPERTY(EditDefaultsOnly, Category="GrabReaction")
	TObjectPtr<UAnimMontage> FallbackVictimMontage;

	/** Section played first — the "being grabbed" pre-pinned pose. Plays on activation. */
	UPROPERTY(EditDefaultsOnly, Category="GrabReaction|Montage Sections")
	FName DefaultSectionName = TEXT("Default");

	/** Section played after Default — the downed/pinned pose. Jumped-to on Default end.
	 *  Loops (alive path: timer-gated to GetupSectionName; dead path: loops until pawn destroy). */
	UPROPERTY(EditDefaultsOnly, Category="GrabReaction|Montage Sections")
	FName PronedSectionName = TEXT("Proned");

	/** Section played after Proned on the alive path — stand-up animation. Ends the ability on complete. */
	UPROPERTY(EditDefaultsOnly, Category="GrabReaction|Montage Sections")
	FName GetupSectionName = TEXT("Getup");

	/** Seconds the Proned section loops before the alive-path Getup jump. Dead-path ignores
	 *  this timer — Proned keeps looping until the respawn pipeline destroys the pawn. */
	UPROPERTY(EditDefaultsOnly, Category="GrabReaction", meta=(ClampMin="0.1", ClampMax="10.0"))
	float ProneDurationAlive = 2.5f;

	/** GE applied on Default→Proned transition. Set to UGE_OSDowned in BP (grants IsDowned +
	 *  Invincibility). Handle stored so EndAbility can remove atomically regardless of exit path. */
	UPROPERTY(EditDefaultsOnly, Category="GrabReaction")
	TSubclassOf<UGameplayEffect> DownedEffect;

private:
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	/** Reaction montage cached from ActivateAbility (selected from TriggerEventData or
	 *  FallbackVictimMontage). Client RPC handler for the alive-path Getup jump reads this to
	 *  target the right montage on the owning client's local AnimInstance. WeakObjectPtr so GC
	 *  can reclaim if the pawn is destroyed mid-reaction. */
	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> CachedMontage;

	/** Timer fires at Default section's natural end (server only). Drives the explicit jump
	 *  to PronedSectionName + apply DownedEffect + remove GrabClaim. Length derived from
	 *  montage->GetSectionLength(DefaultSectionName) at ActivateAbility. */
	FTimerHandle DefaultSectionTimerHandle;

	/** Timer fires after ProneDurationAlive (server only). Drives the alive-path jump to
	 *  GetupSectionName. Dead-path checks IsDead on fire and no-ops (Proned keeps looping). */
	FTimerHandle ProneTimerHandle;

	/** Handle for the applied DownedEffect. Invalidated on EndAbility cleanup regardless of
	 *  exit path — removing the handle atomically clears IsDowned + Invincibility tags. */
	FActiveGameplayEffectHandle DownedEffectHandle;

	/** True if the montage has Default/Proned/Getup sections and the state-machine path is
	 *  active. False means fallback: play once, end on blend-out (pre-Phase-2a behavior). */
	bool bSectionedMontage = false;

	/** Server-only. Called when DefaultSectionTimerHandle fires. Applies DownedEffect, removes
	 *  GrabClaim, jumps montage to PronedSectionName, starts ProneTimerHandle. */
	void OnDefaultSectionEnd();

	/** Server-only. Called when ProneTimerHandle fires. Alive path: jumps to GetupSectionName.
	 *  Dead path: no-op (Proned keeps looping until pawn destroy via respawn pipeline). */
	void OnProneTimerEnd();

	/** Bound to MontageTask->OnCompleted + OnBlendOut. Ends the ability normally when the
	 *  reaction montage finishes (after Getup on alive path, or after the fallback montage
	 *  plays through on the unsectioned path). Does NOT fire mid-montage during section jumps. */
	UFUNCTION()
	void OnReactionMontageEnd();

	/** Server→owning-client Reliable RPC driving the alive-path Getup jump on autonomous proxy.
	 *  Simulated proxies follow the server's jump via RepAnimMontageInfo replication in
	 *  ASC->CurrentMontageJumpToSection. Autonomous proxies skip OnRep_ReplicatedAnimMontage's
	 *  section reconciliation (UE 5.6 IsLocallyControlled gate at
	 *  AbilitySystemComponent_Abilities.cpp:3274), so the owning client needs this explicit RPC
	 *  to trigger its local Montage_JumpToSection. */
	UFUNCTION(Client, Reliable)
	void ClientJumpToGetupSection();
};
