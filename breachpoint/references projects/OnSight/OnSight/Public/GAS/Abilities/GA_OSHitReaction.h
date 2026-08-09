// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "Data/OSDefenseAndReactions.h"
#include "GA_OSHitReaction.generated.h"

class UChooserTable;
class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;

/**
 * Server-authoritative hit reaction: triggered by GameplayEvent.HitReact, applies GE_OSHitReactState,
 * then plays an AnimMontage chosen by the Hit React Chooser table.
 *
 * Chooser setup (e.g. CT_HitReact_Base):
 * - Result Type: Object Of Type, Result Class: AnimMontage
 * - Parameters: one Struct Parameter of type FOSHitReacts (OSHitReacts)
 * - Top-level column: Enum Value bound to ReactType (Light, Heavy, Knockdown, Launch, Death)
 * - Nested choosers (per ReactType): Enum Value bound to HitDirection (FRONT, BACK, LEFT, RIGHT, NONE)
 * Assign this ability's Hit React Chooser in the class defaults to your table.
 */
UCLASS()
class ONSIGHT_API UGA_OSHitReaction : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSHitReaction();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Chooser table (e.g. CT_HitReact_Base) that takes FOSHitReacts and returns AnimMontage. Must use struct type FOSHitReacts. */
	UPROPERTY(EditDefaultsOnly, Category="HitReact", meta=(DisplayName="Hit React Chooser"))
	TObjectPtr<UChooserTable> HitReactChooser = nullptr;

	/** Chooser table used when the character is currently blocking (has IsBlocking tag) */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact", meta = (DisplayName = "Block Hit React Chooser"))
	TObjectPtr<UChooserTable> BlockHitReactChooser = nullptr;

	/** Fallback montage when no chooser is set or the chooser returns nothing. */
	UPROPERTY(EditDefaultsOnly, Category="HitReact")
	TObjectPtr<UAnimMontage> DefaultHitReactMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="HitReact")
	TSubclassOf<UGameplayEffect> HitReactStateEffectClass;

	/** GE applied when ReactType == Knockdown (e.g. BPGE_OSKnockDown). Grants State.KnockedDown for a duration. */
	UPROPERTY(EditDefaultsOnly, Category="HitReact")
	TSubclassOf<UGameplayEffect> KnockDownEffectClass;

private:
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	static EOSHitReactType ReactTypeFromEventMagnitude(float Magnitude);
};

