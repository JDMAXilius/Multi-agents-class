#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "BRGA_Grapple.generated.h"

class ABRWeaponPickup;
class UBRCharacterMovementComponent;

UENUM()
enum class EBRGrappleMode : uint8
{
	None,

	SelfPull,

	WeaponAttract,

	PawnReel
};

UCLASS()
class BREACHPOINT_API UBRGA_Grapple : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Grapple(const FObjectInitializer& ObjectInitializer);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual float GetCooldownDurationSeconds() const override;

private:
	UBRCharacterMovementComponent* GetBRMovement() const;

	bool TraceForTarget(FHitResult& OutHit) const;

	EBRGrappleMode ClassifyHit(const FHitResult& Hit) const;

	bool ValidateTarget(const FHitResult& Claim, EBRGrappleMode ClaimedMode, FString& OutReason) const;

	void BeginPull(const FHitResult& Hit, EBRGrappleMode Mode);

	float GetMaxRangeMetres() const;

	bool bResolved = false;

	EBRGrappleMode ActiveMode = EBRGrappleMode::None;
};
