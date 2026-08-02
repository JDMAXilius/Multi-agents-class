#pragma once

#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "BRAbilitySystemComponent.generated.h"

UCLASS(meta = (DisplayName = "BR Ability System Component"))
class BREACHPOINT_API UBRAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UBRAbilitySystemComponent(const FObjectInitializer& ObjectInitializer);

	void AbilityInputTagPressed(FGameplayTag InputTag);

	void AbilityInputTagReleased(FGameplayTag InputTag);

	void ClearAbilityInput();

	bool IsAbilityInputHeld(FGameplayTag InputTag) const { return HeldInputTags.Contains(InputTag); }

	const TArray<FGameplayTag>& GetHeldInputTags() const { return HeldInputTags; }

	void ExecuteInPredictionWindow(TFunctionRef<void()> Work);

	bool BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle AbilityHandle, bool bEndAbilityImmediately = false);

	virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }

	bool ApplyRecentDamageGate();

	void SetShieldsBrokenState(bool bBroken);

	bool ApplyInitStats();

	bool ApplyDeathEffect();

	void ClearDeathEffect();

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Abilities")
	TSubclassOf<UGameplayEffect> RecentDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Abilities")
	TSubclassOf<UGameplayEffect> ShieldsBrokenEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Abilities")
	TSubclassOf<UGameplayEffect> InitStatsEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Abilities")
	TSubclassOf<UGameplayEffect> ShieldRegenEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Abilities")
	TSubclassOf<UGameplayEffect> DeathEffectClass;

private:
	bool HasActiveEffectOfClass(TSubclassOf<UGameplayEffect> EffectClass) const;

	void InvokeInputEventForSpec(const FGameplayAbilitySpec& Spec, EAbilityGenericReplicatedEvent::Type EventType);

	TArray<FGameplayTag> HeldInputTags;
};
