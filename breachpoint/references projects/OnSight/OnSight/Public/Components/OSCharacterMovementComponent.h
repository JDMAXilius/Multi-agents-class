// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Movement: event-driven MaxWalkSpeed from UOSAttributeSet.
// Server-side: suppresses CMC corrections during attacks (ServerCheckClientError) + post-attack grace period.
// Proxy: zeroes local RM translation during attacks (ConstrainAnimRootMotionVelocity) so the engine's
// fixup (SimulatedRootMotionPositionFixup + SmoothCorrection) is the sole position driver.

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectTypes.h"
#include "OSCharacterMovementComponent.generated.h"

class UOSAttributeSet;
class UAbilitySystemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ONSIGHT_API UOSCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UOSCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/** Server only. Call when an attack ability ends (e.g. GA_OSAttack::EndAbility) so CMC can ease
	 *  back into ServerCheckClientError without a single huge ClientAdjustPosition (#19). */
	void OnServerPostAttackMovementResume();
	
	//UPROPERTY(Transient)
	//bool bSuppressMontageRootMotionTranslation = false;

	// ========================================
	// MOVEMENT CONFIGURATION
	// ========================================
	
	/** Fallback base walk speed if attributes aren't ready yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	float BaseWalkSpeed = 500.0f;

	/** After an attack ends, server continues to skip ClientAdjustPosition generation for this many
	 *  seconds so incoming client moves can pull authoritative state back in line (#19 snap). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Combat", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PostAttackCorrectionGraceSeconds = 0.30f;
	
	/** Bind to movement attributes changes */
	void BindMovementAttributes(UAbilitySystemComponent* InAbilitySystemComponent);
	
	/** Recompute MaxWalkSpeed from current attributes (no-op if attributes not ready). */
	void RefreshMovementSpeed();
	
	/** Gameplay tags that block movement (e.g., Stun, Death) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	FGameplayTagContainer MovementBlockingTags;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	/** Unbind ASC attribute delegates before destruction. CMC lives on Character but binds
	 *  to delegates on the PlayerState-owned ASC, which outlives Character across respawn.
	 *  Without this cleanup, the ASC fires attribute change delegates into a destroyed CMC
	 *  (use-after-free). Fires on component unregister/actor destroy. */
	virtual void UninitializeComponent() override;

	/** Simulated proxy during attacks: zero out local root motion translation so the fixup
	 *  (SimulatedRootMotionPositionFixup + SmoothCorrection) is the sole position driver.
	 *  Returns ZeroVector (not CurrentVelocity — that was the original bug that caused jitter
	 *  by substituting walking velocity between fixup ticks). */
	virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;

	// Override to calculate speed from MovementSpeed attribute
	virtual float GetMaxSpeed() const override;
	
	// Calculate movement speed from attribute
	float CalculateMovementSpeed() const;
	
	// Handle MovementSpeed attribute changes
	void Internal_OnMovementSpeedChanged(const FOnAttributeChangeData& Data);
	
	/** Server-side: skip error checks during attacks so the server doesn't fight motion warps.
	 *  Uses the engine's designed extension point — prevents corrections from being generated,
	 *  rather than ignoring them on the client after they arrive (the previous approach via
	 *  SmoothClientPosition/ClientAdjustPosition overrides, which wasted bandwidth and left stale state). */
	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;

	// Get attribute set
	UOSAttributeSet* GetAttributeSet() const;

private:
	TWeakObjectPtr<UOSAttributeSet> AttributeSet;

	/** Cached ASC weak ref captured in BindMovementAttributes so UninitializeComponent can unbind
	 *  even if the owner is already partially torn down. ASC lives on PlayerState and outlives
	 *  the Character across respawn, so we cannot rely on re-looking it up via the owner. */
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	/** Server world time (GetTimeSeconds) until which ServerCheckClientError stays suppressed after
	 *  an attack ends; <= 0 means inactive. */
	float PostAttackServerCorrectionGraceUntilTime = -1.f;
};
