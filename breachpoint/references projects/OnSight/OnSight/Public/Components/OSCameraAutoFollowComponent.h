// Chooser-driven camera auto-follow component.
// Evaluates a Chooser Table each tick to resolve camera parameters from gameplay tag state.
// Handles: auto-follow behind, locomotion sweep, combat soft-lock sweep, state-driven zoom.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OSCameraStates.h"
#include "OSCameraAutoFollowComponent.generated.h"

class APlayerController;
class APawn;
class USpringArmComponent;
class UCameraComponent;
class UAbilitySystemComponent;
class UChooserTable;
class UOSCameraConfigDataAsset;

/**
 * Chooser-driven automatic camera yaw + zoom behaviors for third-person combat.
 *
 * Architecture:
 *   - Each tick, builds FOSCameraStateParams from current gameplay tags on the ASC.
 *   - Evaluates a UChooserTable with those params to resolve a UOSCameraConfigDataAsset.
 *   - Interpolates spring arm / camera values toward the resolved config.
 *   - Runs auto-behaviors (sweep, follow) using speeds from the active config.
 *
 * Golden Rule:
 *   Player camera input (right stick / mouse look) ALWAYS takes priority.
 *   Auto-behaviors only run when the player has not touched the camera for InputGracePeriod.
 *
 * Attach to AOSPlayerController (same owner pattern as OSDynamicCameraComponent).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ONSIGHT_API UOSCameraAutoFollowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSCameraAutoFollowComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ========================================
	// CHOOSER TABLE
	// ========================================

	/** The Chooser Table asset that maps gameplay tag states to camera configs.
	 *  Columns should match FOSCameraStateParams fields.
	 *  Result type should be UOSCameraConfigDataAsset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Chooser")
	TObjectPtr<UChooserTable> CameraChooserTable;

	/** Fallback camera config used when the Chooser Table is missing or returns null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Chooser")
	TObjectPtr<UOSCameraConfigDataAsset> FallbackConfig;

	// ========================================
	// PLAYER INPUT DETECTION
	// ========================================

	/** Call this every frame the player provides camera look input. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Input")
	void NotifyCameraInput();

	/** How long after the last camera input before auto-behaviors resume (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Input")
	float InputGracePeriod = 0.3f;

	// ========================================
	// COMBAT SWEEP API
	// ========================================

	/** Call when an attack begins against a soft-targeted enemy.
	 *  The camera will sweep incrementally toward TargetActor using active config values. */
	UFUNCTION(BlueprintCallable, Category = "Camera|CombatSweep")
	void NotifyAttackOnTarget(AActor* TargetActor);

	/** Clear any active combat sweep (target lost, combat ended, etc.). */
	UFUNCTION(BlueprintCallable, Category = "Camera|CombatSweep")
	void ClearCombatSweep();

	// ========================================
	// ZOOM API (Manual Override)
	// ========================================

	/** Manually request a zoom-in (override Chooser-driven boom length).
	 *  Reverts to Chooser-driven values when ClearZoomOverride is called. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Zoom")
	void ZoomIn(float TargetBoomLength, float TargetFOV, float TransitionSpeed = 8.0f);

	/** Manually request a zoom-out (override Chooser-driven boom length). */
	UFUNCTION(BlueprintCallable, Category = "Camera|Zoom")
	void ZoomOut(float TargetBoomLength, float TargetFOV, float TransitionSpeed = 8.0f);

	/** Clear manual zoom override; resume Chooser-driven values. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Zoom")
	void ClearZoomOverride();

	// ========================================
	// STATE QUERY
	// ========================================

	/** Returns the currently active camera config (resolved from Chooser or fallback). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Camera|State")
	const UOSCameraConfigDataAsset* GetActiveConfig() const { return ActiveConfig; }

	/** Returns true if the player is currently providing camera input. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Camera|State")
	bool IsPlayerControllingCamera() const;

	// ========================================
	// DEBUG
	// ========================================

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebugDraw = false;

private:
	// --- Cached references ---
	UPROPERTY()
	TObjectPtr<APlayerController> OwnerPC;

	UPROPERTY()
	TObjectPtr<APawn> CachedPawn;

	UPROPERTY()
	TObjectPtr<USpringArmComponent> CachedBoom;

	UPROPERTY()
	TObjectPtr<UCameraComponent> CachedCamera;

	// --- Active config ---
	UPROPERTY()
	TObjectPtr<const UOSCameraConfigDataAsset> ActiveConfig;

	// --- Input tracking ---
	float TimeSinceLastCameraInput = 10.0f;

	// --- Movement tracking ---
	float TimeSinceLastMovement = 0.0f;

	// --- Locomotion sweep state ---
	float CurrentSweepSpeed = 0.0f;

	// --- Combat sweep state ---
	TWeakObjectPtr<AActor> CombatSweepTarget;
	float CombatSweepRemaining = 0.0f;

	// --- Zoom override ---
	bool bZoomOverrideActive = false;
	float ZoomOverrideBoomLength = 0.0f;
	float ZoomOverrideFOV = 0.0f;
	float ZoomOverrideTransitionSpeed = 8.0f;

	// --- Core pipeline ---
	void CacheReferences();
	FOSCameraStateParams BuildStateParams() const;
	UOSCameraConfigDataAsset* EvaluateChooser(const FOSCameraStateParams& Params) const;
	void ApplyConfig(float DeltaTime);

	// --- Behaviors ---
	void UpdateAutoFollow(float DeltaTime);
	void UpdateLocomotionSweep(float DeltaTime);
	void UpdateCombatSweep(float DeltaTime);

	// --- Helpers ---
	float GetYawDeltaToTarget(float TargetYaw) const;
	bool GetCharacterMovementYaw(float& OutYaw) const;
	void ApplyYawToController(float YawDelta) const;
	float GetCameraYaw() const;
	float GetCharacterForwardYaw() const;

	void DrawDebugInfo() const;
};
