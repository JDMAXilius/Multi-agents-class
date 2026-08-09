#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GA_OSMelee.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_ApplyRootMotionMoveToActorForce;
class UAT_OSTickAbilityTask;
class UAnimMontage;
class UCurveVector;
class UGameplayEffect;
class AOSCharacter;

/** Reference-style melee: optional motion-warp + root-motion gap close, montage, hit-window tick sweep, server damage. */
UENUM(BlueprintType)
enum class EOSWeaponMetrics : uint8
{
	Rank UMETA(DisplayName = "Damage (Rank)"),
	SwingThreashold UMETA(DisplayName = "Swing Threshold A"),
	SwingTreshold UMETA(DisplayName = "Swing Threshold B"),
	MeleeRange UMETA(DisplayName = "Melee Range (trace)"),
	WeakPoint UMETA(DisplayName = "Weak Point Mult"),
	MAX UMETA(Hidden)
};

UCLASS()
class ONSIGHT_API UGA_OSMelee : public UOSGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_OSMelee();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Per-weapon tuning (reference EWeaponMetrics). Index with EOSWeaponMetrics. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Metrics")
	TArray<float> WeaponMetrics;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	UAnimMontage* MontageToPlay = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|MotionWarp")
	FName MotionWarpTargetName = FName(TEXT("MeleeWarpTarget"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|MotionWarp")
	bool bUseRootMotionGapClose = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|MotionWarp", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float RootMotionDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|MotionWarp", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float ReachedDestinationDistance = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|MotionWarp")
	TObjectPtr<UCurveVector> PathOffsetCurve = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Trace")
	float SweepSphereRadius = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Trace", meta = (ClampMin = "0.0"))
	float SweepTraceZOffset = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Events")
	FGameplayTag HitWindowStartTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Events")
	FGameplayTag HitWindowEndTag;

	/** Optional explicit target; if null, only warp/root-motion toward soft-lock can be wired later. */
	UPROPERTY(BlueprintReadWrite, Category = "Melee")
	TWeakObjectPtr<AActor> MeleeTarget;

	float GetWeaponMetric(EOSWeaponMetrics Index) const;

	UFUNCTION()
	void OnMeleeHitWindowStart(FGameplayEventData Payload);

	UFUNCTION()
	void OnMeleeHitWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnMeleeSweepTick(float DeltaTime);

	UFUNCTION()
	void OnMeleeMontageEnded();

	UFUNCTION()
	void OnMeleeMontageInterrupted();

	UFUNCTION()
	void OnRootMotionMoveFinished(bool bDestinationReached, bool bTimedOut, FVector FinalTargetLocation);

private:
	void OnMotionWarpTarget();
	void SetupMotionWarpAndMoveToTarget();
	void ClearMeleeWarp();
	void TryApplyMeleeDamage(AOSCharacter* Victim);

	UPROPERTY()
	TArray<AActor*> HitActorsThisSwing;

	UPROPERTY()
	TWeakObjectPtr<AActor> ValidTarget;

	UPROPERTY()
	TObjectPtr<UAT_OSTickAbilityTask> ActiveSweepTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_ApplyRootMotionMoveToActorForce> ActiveRootMotionTask = nullptr;
};
