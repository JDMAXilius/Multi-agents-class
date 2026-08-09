// Copyright Epic Games, Inc. All Rights Reserved.
// Event-driven, Chooser-based camera: no Tick, no hardcoded tags. Config drives tags to observe and Chooser table resolves preset.
// Client-only. Abilities never touch camera; they only grant/remove tags.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "OSCameraModeComponent.generated.h"

class APlayerController;
class APawn;
class UAbilitySystemComponent;
class USpringArmComponent;
class UCameraComponent;
class UChooserTable;

// ========== Chooser input: tag state + threat count (no hardcoded tags) ==========
USTRUCT(BlueprintType)
struct ONSIGHT_API FOSCameraModeChooserContext
{
	GENERATED_BODY()

	/** Owned gameplay tags from the character's ASC. Chooser columns evaluate these. */
	UPROPERTY(BlueprintReadOnly, Category = "Camera Mode")
	FGameplayTagContainer ActiveTags;

	/** Threat count from engagement scan. Chooser can branch on 0, 1, 2+. */
	UPROPERTY(BlueprintReadOnly, Category = "Camera Mode")
	int32 ThreatCount = 0;
};

// ========== Preset (data asset for designer iteration) ==========
UCLASS(BlueprintType)
class ONSIGHT_API UOSCameraPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Camera arm pose: Location.X = arm length, Location.Y/Z = socket offset (side/height). Rotation = additive pitch/yaw/roll offset. */
	UPROPERTY(EditDefaultsOnly, Category = "Spring Arm")
	FTransform CameraArmTransform = FTransform(FVector(400.f, 0.f, 0.f));

	UPROPERTY(EditDefaultsOnly, Category = "Spring Arm")
	bool bUsePawnControlRotation = true;

	UPROPERTY(EditDefaultsOnly, Category = "Spring Arm")
	bool bEnableCameraLag = true;

	UPROPERTY(EditDefaultsOnly, Category = "Spring Arm")
	float CameraLagSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Spring Arm")
	bool bEnableCameraRotationLag = true;

	UPROPERTY(EditDefaultsOnly, Category = "Spring Arm")
	float CameraRotationLagSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float FieldOfView = 90.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float BlendTimeSeconds = 0.3f;
};

// ========== Engagement tier (local threat count) ==========
UENUM(BlueprintType)
enum class EOSEngagementTier : uint8
{
	None     UMETA(DisplayName = "No Threats"),
	Single   UMETA(DisplayName = "1 Enemy"),
	Multiple UMETA(DisplayName = "2+ Enemies")
};

// ========== Config: Chooser + tags to observe + tuning (all data-driven) ==========
//
// CHOOSER TABLE SETUP (Editor):
// 1. Create a Chooser Table asset (e.g. CT_CameraMode).
// 2. Set the Chooser's Context (input) to FOSCameraModeChooserContext:
//    - In the Chooser editor, set "Context Type" / input struct to FOSCameraModeChooserContext.
// 3. Add columns to match state (order = priority, first match wins):
//    - Use "Gameplay Tag" or "Gameplay Tag Container" columns to test ActiveTags (e.g. HasTag Dead → row Dead).
//    - Use "Compare Int" or "Property" columns to test ThreatCount (0, 1, 2+).
//    - Typical order: Dead, Stunned, Airborne, Dodging, LockedOn, Sprinting, Blocking, Attacking, Engaged_Multi (ThreatCount>=2), Engaged_1v1, Default.
// 4. Set the Result (output) column type to Object; assign a UOSCameraPreset asset per row.
// 5. Assign this Chooser to CameraModeChooserTable on UOSCameraModeConfig.
//
UCLASS(BlueprintType)
class ONSIGHT_API UOSCameraModeConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Chooser Table: assign your CT_CameraMode (or similar) here. Input FOSCameraModeChooserContext, output UOSCameraPreset*. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera Mode", meta = (DisplayName = "Camera Mode Chooser Table"))
	TObjectPtr<UChooserTable> CameraModeChooserTable;

	/** Tags to listen to. When any of these change, Chooser is re-evaluated and preset applied. No hardcoded tags in code. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera Mode")
	TArray<FGameplayTag> TagsToObserve;

	/** Preset used when Chooser returns no result. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera Mode")
	TObjectPtr<UOSCameraPreset> FallbackPreset;

	/** Tags that count as "in combat" for engagement tier (threat count). If any present, threat count is used in Chooser params. */
	UPROPERTY(EditDefaultsOnly, Category = "Engagement")
	TArray<FGameplayTag> CombatTagsForEngagement;

	/** Camera preset when exactly one threat is in range (EOSEngagementTier::Single). If set, overrides Chooser when tier is Single. */
	UPROPERTY(EditDefaultsOnly, Category = "Engagement", meta = (DisplayName = "Preset: One Target"))
	TObjectPtr<UOSCameraPreset> PresetOneTarget;

	/** Camera preset when two or more threats are in range (EOSEngagementTier::Multiple). If set, overrides Chooser when tier is Multiple. */
	UPROPERTY(EditDefaultsOnly, Category = "Engagement", meta = (DisplayName = "Preset: Multiple Targets"))
	TObjectPtr<UOSCameraPreset> PresetMultipleTargets;

	/** Actors with any of these tags are excluded from threat count (e.g. Dead). */
	UPROPERTY(EditDefaultsOnly, Category = "Engagement")
	TArray<FGameplayTag> TagsThatExcludeFromThreatCount;

	/** Optional. When this tag is added and FocusTarget is set, lock-on rotation timer runs; when removed, timer stops. */
	UPROPERTY(EditDefaultsOnly, Category = "Lock-On")
	FGameplayTag LockOnTag;

	UPROPERTY(EditDefaultsOnly, Category = "Engagement", meta = (ClampMin = "0.0"))
	float EngagementScanRadius = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Engagement", meta = (ClampMin = "0.0"))
	float TierDowngradeDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lock-On")
	bool bEnableLockOnTracking = true;

	UPROPERTY(EditDefaultsOnly, Category = "Lock-On", meta = (EditCondition = "bEnableLockOnTracking", ClampMin = "0.1"))
	float LockOnRotationSpeed = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lock-On", meta = (EditCondition = "bEnableLockOnTracking"))
	float LockOnVerticalOffset = 50.0f;

	/** If true, preset values (arm length, FOV, offset, lag) interpolate over BlendTimeSeconds; if false, apply instantly. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	bool bSmoothPresetTransitions = true;

	/** Timer interval for lock-on and preset blend updates (no Tick). ~60 Hz if 0.016f. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (ClampMin = "0.001"))
	float CameraTimerInterval = 0.016f;
};

// ========== Camera mode component ==========
/**
 * Event-driven, Chooser-based camera preset resolution (arm length, FOV, lag, lock-on).
 * Attach to AOSPlayerController (same owner pattern as OSCameraAutoFollowComponent).
 * Gets SpringArm/Camera from the possessed pawn and ASC from the pawn (e.g. PlayerState).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ONSIGHT_API UOSCameraModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSCameraModeComponent();

	// --- Config ---
	/** Assign a UOSCameraModeConfig Data Asset. The Chooser Table (and Tags To Observe, Fallback Preset) are set on that config, not here. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera Mode", meta = (DisplayName = "Camera Mode Config"))
	TObjectPtr<UOSCameraModeConfig> CameraModeConfig;

	// --- Initialization ---
	/** Call when the owner PC possesses a pawn (e.g. from OnPossess). Re-caches pawn/SpringArm/Camera/ASC and re-inits tag listeners and evaluation. */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void OnPawnPossessed();

	// --- Lock-on / focus ---
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetFocusTarget(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ClearFocusTarget() { FocusTarget = nullptr; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	AActor* GetFocusTarget() const { return FocusTarget; }

	// --- Queries ---
	UFUNCTION(BlueprintPure, Category = "Camera")
	UOSCameraPreset* GetCurrentPreset() const { return CurrentPreset; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	EOSEngagementTier GetEngagementTier() const { return CurrentEngagementTier; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY()
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	/** Owner is the PlayerController (same as OSCameraAutoFollowComponent). */
	UPROPERTY()
	TObjectPtr<APlayerController> OwnerPC;

	/** Possessed pawn; SpringArm and Camera are on this. */
	UPROPERTY()
	TObjectPtr<APawn> CachedPawn;

	UPROPERTY()
	TObjectPtr<AActor> FocusTarget;

	UPROPERTY()
	TObjectPtr<UOSCameraPreset> CurrentPreset;

	EOSEngagementTier CurrentEngagementTier;
	float LastEngagementUpdateTime = 0.f;
	FTimerHandle LockOnUpdateTimer;
	FTimerHandle PresetBlendTimer;
	FTimerHandle DelayedEngagementCheckTimer;
	float TimeInLowerTier;

	TMap<FGameplayTag, FDelegateHandle> TagDelegateHandles;

	void CacheReferences();
	void RegisterTagListeners();
	void UnregisterTagListeners();
	void RegisterListenersAndEvaluate();
	void OnObservedTagChanged(const FGameplayTag Tag, int32 NewCount);

	void ResolveAndApplyPreset(float DeltaTimeForEngagement = 0.f);
	FOSCameraModeChooserContext BuildChooserContext(int32 ThreatCount) const;
	UOSCameraPreset* ResolvePresetFromChooser(const FOSCameraModeChooserContext& Context) const;
	void ApplyPreset(UOSCameraPreset* Preset);
	void OnPresetBlendTick();
	void StopPresetBlendTimer();

	void UpdateLockOnCamera(float DeltaTime);
	void OnLockOnTimerTick();
	FRotator GetLockOnTargetRotation(AActor* Target) const;
	void StartLockOnTimerIfNeeded();
	void StopLockOnTimer();

	void UpdateEngagementTier(float DeltaTime, int32 ThreatCount);
	EOSEngagementTier ComputeEngagementTierFromCount(int32 ThreatCount) const;
	int32 CountThreatsInRadius(float Radius) const;
	void OnDelayedEngagementCheck();
};
