#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_OSSoftLock.generated.h"

class UGameplayEffect;

/**
 * Port of reference GA_Weapon_ToggleTracking (IMG_5202).
 * Toggles infinite tracking GE: if ASC has TrackingStateTag, RemoveActiveEffectsWithGrantedTags; else ApplyTrackingEffectClass at TrackingEffectLevel.
 */
UCLASS()
class ONSIGHT_API UGA_OSSoftLock : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSSoftLock();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tracking")
	FGameplayTag TrackingStateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tracking")
	TSubclassOf<UGameplayEffect> TrackingEffectClass;

	/** Level 0.5 — exact value from reference Apply GE node (IMG_5202). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tracking")
	float TrackingEffectLevel = 0.5f;

private:
	void RemoveTrackingEffect(UAbilitySystemComponent* ASC);
	void ApplyTrackingEffect(UAbilitySystemComponent* ASC);
};
