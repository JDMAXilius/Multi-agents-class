#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "GameplayEffectTypes.h"

#include "BRGA_Melee.generated.h"

class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;

UCLASS()
class BREACHPOINT_API UBRGA_Melee : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Melee(const FObjectInitializer& ObjectInitializer);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:

	UFUNCTION()
	void OnWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnSwingWatchdogElapsed();

	UFUNCTION()
	void OnClaimTimeoutElapsed();

	bool GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	FHitResult SweepSwing(const FVector& From, const FVector& To, float SweepRadiusUU) const;

	void ResolveSwing();

	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag);

	bool ValidateClaim(const FHitResult& Claim, float RangeUU, FString& OutReason) const;

	bool IsRearHit(const AActor* Victim, float RearArcDegrees) const;

	void ApplyMeleeDamage(const FHitResult& Hit, float BaseDamage, bool bRearHit) const;

	void ClearTasks();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WindowBeginTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WindowEndTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> WatchdogTask;

	FDelegateHandle TargetDataDelegateHandle;

	FVector SwingOriginLocation = FVector::ZeroVector;

	bool bWindowOpen = false;

	bool bSwingResolved = false;

	bool bClaimHandled = false;
};
