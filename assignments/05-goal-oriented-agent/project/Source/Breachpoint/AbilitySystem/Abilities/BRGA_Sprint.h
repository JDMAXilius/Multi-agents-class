#pragma once

#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "BRGA_Sprint.generated.h"

class UBRCharacterMovementComponent;

UCLASS(meta = (DisplayName = "BRGA_Sprint"))
class BREACHPOINT_API UBRGA_Sprint : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Sprint(const FObjectInitializer& ObjectInitializer);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// Bounds how long a stationary sprint can persist. Leaving the ground is NOT polled at this
	// rate - it arrives on the movement-mode delegate the frame it happens.
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Sprint")
	float SprintCheckIntervalSeconds = 0.1f;

	// Planar speed below which the sprint counts as stopped. Compared squared.
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Sprint")
	float MinSprintSpeedUU = 10.f;

private:
	UBRCharacterMovementComponent* GetBRCharacterMovement(const FGameplayAbilityActorInfo* ActorInfo) const;

	ACharacter* GetAvatarCharacter() const;

	void StartSprintCheckLoop();

	bool IsMovingFastEnough() const;

	// Both handlers are UFUNCTION because the delegates that call them are dynamic.
	UFUNCTION()
	void OnSprintCheck();

	UFUNCTION()
	void HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	void CancelSelf();

	bool bBoundMovementMode = false;
};
