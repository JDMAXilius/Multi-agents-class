// Breachpoint. The ability base: activation policy, generic cooldown, cancel hygiene, one death gate.
#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "BRGameplayAbility.generated.h"

class UBRAbilitySystemComponent;

UENUM(BlueprintType)
enum class EBRAbilityActivationPolicy : uint8
{
	OnInputPressed,

	WhileInputHeld,

	OnSpawn
};

UCLASS(Abstract, meta = (DisplayName = "BR Gameplay Ability"))
class BREACHPOINT_API UBRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGameplayAbility(const FObjectInitializer& ObjectInitializer);

	void ExternalEndAbility();

	EBRAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	UBRAbilitySystemComponent* GetBRAbilitySystemComponent() const;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual float GetCooldownDurationSeconds() const { return 0.f; }

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Ability")
	EBRAbilityActivationPolicy ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Ability")
	FGameplayTag CooldownTag;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Ability")
	bool bCommitOnActivate = true;

private:
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	mutable FGameplayTagContainer CooldownTagsScratch;
};
