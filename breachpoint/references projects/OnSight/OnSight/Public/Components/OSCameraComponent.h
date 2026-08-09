// Copyright Epic Games, Inc. All Rights Reserved.
// Chooser-driven, tag-reactive camera preset blending.
// Attach to APlayerController. No lock-on. No engagement. No combat sweep.
// Tick drives blending only — disabled at rest.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Data/OSCameraStates.h"
#include "OSCameraComponent.generated.h"

class APlayerController;
class APawn;
class AOSPlayerState;
class USpringArmComponent;
class UCameraComponent;
class UAbilitySystemComponent;
class UChooserTable;

DEFINE_LOG_CATEGORY_STATIC(LogOSCamera, Log, All);

// =============================================================================
// OSCameraComponent
//
// One component. One job: resolve the correct UOSCameraConfigDataAsset from the
// pawn's gameplay tag state via a Chooser Table, then blend toward it smoothly.
//
// Flow:
//   BeginPlay → bind OnPossessedPawnChanged; BindToPlayerStateGASReady (subscribe to PS->OnGASReady for ASC).
//   Possession → CacheReferences(Old,New): pawn/SpringArm/Camera, SnapToPreset(Default). ASC from OnGASReady only.
//   OnGASReady (from PlayerState) → ASC + RegisterTagListeners. Tick only while blending.
//
// =============================================================================

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ONSIGHT_API UOSCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSCameraComponent();

	// ---- Config ----

	/**
	 * Chooser Table asset.
	 * Input:  FOSCameraStateParams
	 * Output: UOSCameraConfigDataAsset*
	 * Row order = priority. First match wins.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TObjectPtr<UChooserTable> CameraChooserTable;

	/** Fallback config when Chooser is missing or when all observed tags are cleared. Must be set. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TObjectPtr<UOSCameraConfigDataAsset> DefaultPreset;

	/** Tags the component listens to on the ASC (e.g. Camera.State.Sprinting, Camera.State.Attacking). When any changes, Chooser re-evaluates. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TArray<FGameplayTag> TagsToObserve;

	// ---- Queries ----

	UFUNCTION(BlueprintPure, Category = "Camera")
	const UOSCameraConfigDataAsset* GetActivePreset() const { return TargetPreset.Get(); }

	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsBlending() const { return bIsBlending; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ---- Cached references ----

	UPROPERTY()
	TObjectPtr<APlayerController> OwnerPC;

	UPROPERTY()
	TObjectPtr<AOSPlayerState> CachedPlayerState;

	UPROPERTY()
	TObjectPtr<APawn> CachedPawn;

	UPROPERTY()
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ASC;

	// ---- Preset state ----

	UPROPERTY()
	TObjectPtr<const UOSCameraConfigDataAsset> TargetPreset;

	// Snapshot of spring arm / camera values at the moment a blend begins.
	// Interpolated FROM these values toward TargetPreset values.
	float BlendFromArmLength         = 350.f;
	float BlendFromHeightOffset      = 60.f;
	float BlendFromSideOffset        = 0.f;
	float BlendFromPitchOffset       = 0.f;
	float BlendFromFOV               = 90.f;
	float BlendFromLagSpeed          = 10.f;
	float BlendFromRotationLagSpeed  = 10.f;

	// ---- Blend state ----

	bool  bIsBlending    = false;
	float BlendElapsed   = 0.f;   // seconds elapsed in current blend
	float BlendDuration  = 0.f;   // BlendTime from TargetPreset

	// ---- Tag listeners ----

	TMap<FGameplayTag, FDelegateHandle> TagHandles;

	FTimerHandle PSBindTimer;

	// ---- Internal pipeline ----

	/** Set ASC and register tag listeners. Used by BindToPlayerStateGASReady and OnPlayerStateGASReady. */
	void ApplyASC(UAbilitySystemComponent* NewASC);

	/** Subscribe to OwnerPC's PlayerState OnGASReady once; retries on timer until PS exists (client). */
	void BindToPlayerStateGASReady();

	UFUNCTION()
	void OnPlayerStateGASReady(UAbilitySystemComponent* ReadyASC);

	/** On possession: update pawn/SpringArm/Camera, snap to default. ASC comes from OnGASReady only. */
	UFUNCTION()
	void CacheReferences(APawn* OldPawn, APawn* NewPawn);

	/** Register tag change delegates on the ASC for all TagsToObserve. */
	void RegisterTagListeners();

	/** Remove all tag change delegates. */
	void UnregisterTagListeners();

	/** Called when any observed tag count changes. Tag added → resolve and blend; tag removed → blend to DefaultPreset. */
	void OnTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Run the Chooser with current ASC tags. Returns DefaultPreset if no match. */
	UOSCameraConfigDataAsset* ResolvePreset() const;

	/**
	 * Snapshot current live spring arm / camera values as blend-from origin,
	 * set TargetPreset, reset blend elapsed, enable Tick.
	 * If already blending, snapshot is from in-progress state so transition stays smooth.
	 */
	void BeginBlendTo(const UOSCameraConfigDataAsset* Preset);

	/**
	 * Tick-callable blend update. Accumulates DeltaTime, samples curve alpha,
	 * lerps all values. Isolated so the callsite can be swapped (timer, etc.) later.
	 */
	void TickBlend(float DeltaTime);

	/** Apply all values from Preset directly with no interpolation. Used on init. */
	void SnapToPreset(const UOSCameraConfigDataAsset* Preset) const;

	/** Apply lerped values to SpringArm and Camera given a [0..1] alpha. */
	void ApplyBlendAlpha(float Alpha) const;
};