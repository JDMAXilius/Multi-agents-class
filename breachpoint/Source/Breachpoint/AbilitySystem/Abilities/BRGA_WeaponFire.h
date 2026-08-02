// BREACHPOINT — BP03 step 2. The fire path.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "BRGA_WeaponFire.generated.h"

class UBRWeaponInstance;
struct FBRWeaponRow;

UCLASS()
class BREACHPOINT_API UBRGA_WeaponFire : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_WeaponFire(const FObjectInitializer& ObjectInitializer);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual float GetCooldownDurationSeconds() const override;

private:
	UBRWeaponInstance* GetActiveWeapon() const;

	const FBRWeaponRow* GetWeaponRow() const;

	bool GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	FHitResult TraceShot(const FBRWeaponRow& Row, const FVector& From, const FVector& Direction) const;

	void FireLocally();

	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag);

	bool ValidateClaim(const FHitResult& Claim, const FBRWeaponRow& Row, FString& OutReason) const;

	void ApplyDamage(const FHitResult& Hit, const FBRWeaponRow& Row) const;

	FDelegateHandle TargetDataDelegateHandle;

	bool bShotResolved = false;
};
