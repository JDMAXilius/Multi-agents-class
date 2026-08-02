// BREACHPOINT — BP05 step 1. The grenade: cook, throw, and the server-only projectile spawn.
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"

#include "BRGA_Grenade.generated.h"

class UAbilitySystemComponent;
class UAbilityTask_WaitInputRelease;

USTRUCT()
struct FBRGrenadeTuning
{
	GENERATED_BODY()

	float CookSeconds = 0.f;

	float FuseSeconds = 0.f;

	float ThrowSpeedMetresPerSecond = 0.f;

	float BlastRadiusMetres = 0.f;

	float BlastCentreDamage = 0.f;

	float Bounciness = -1.f;

	float BounceFriction = -1.f;
};

UCLASS()
class BREACHPOINT_API UBRGA_Grenade : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Grenade(const FObjectInitializer& ObjectInitializer);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
	FGameplayEffectSpecHandle MakeCostSpec() const;

	UFUNCTION()
	void HandleCookReleased(float TimeHeld);

	void HandleCookExpired();

	bool ResolveTuning(FBRGrenadeTuning& OutTuning, FString& OutReason) const;

	void ThrowGrenade(float SecondsCooked);

	void RequestProjectileSpawn(const FTransform& ReleaseTransform, const FVector& LaunchVelocity, float RemainingFuseSeconds) const;

	void ClearCook();

	bool GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	static FGameplayTag RequestOwedTag(const TCHAR* TagString);

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> CookReleaseTask;

	FTimerHandle CookTimerHandle;

	FBRGrenadeTuning Tuning;

	bool bThrowResolved = false;
};
