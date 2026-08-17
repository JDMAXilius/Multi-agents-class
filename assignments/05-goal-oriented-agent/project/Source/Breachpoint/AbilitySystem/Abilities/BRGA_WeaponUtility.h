#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "BRGA_WeaponUtility.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UBREquipmentComponent;
class UBRWeaponInstance;
struct FBRWeaponRow;

UCLASS()
class BREACHPOINT_API UBRGA_Reload : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Reload(const FObjectInitializer& ObjectInitializer);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void OnReloadCommit(FGameplayEventData Payload);

	UFUNCTION()
	void OnFallbackElapsed();

	void Commit();

	bool bCommitted = false;
};

UCLASS()
class BREACHPOINT_API UBRGA_WeaponSwap : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_WeaponSwap(const FObjectInitializer& ObjectInitializer);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void OnSwapCommit(FGameplayEventData Payload);

	UFUNCTION()
	void OnFallbackElapsed();

	void Commit();

	bool bCommitted = false;
};
