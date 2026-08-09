// Chooser-driven camera auto-follow component implementation.
//
// REVIEW (Right vs Wrong):
// RIGHT: PC ownership + cached pawn/boom/camera (same pattern as OSCameraModeComponent); Chooser for state->config; player input grace period; combat/locomotion/auto-follow behaviors; zoom override API; debug draw in shipping guard.
// WRONG: Tick every frame (evaluates Chooser + ApplyConfig every frame); no tag-driven events (could be event-driven like OSCameraModeComponent); CacheReferences() called every Tick; LogTemp for missing SpringArm; ApplyConfig always runs even when config unchanged; socket offset only applied when !bZoomOverrideActive (zoom override ignores offset).

#include "Components/OSCameraAutoFollowComponent.h"
#include "Utilities/ChooserHelper.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"

UOSCameraAutoFollowComponent::UOSCameraAutoFollowComponent()
{
	// WRONG: Tick every frame is costly; Chooser + ApplyConfig could run only on tag change (event-driven).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

// =============================================================================
// Input / Combat API
// =============================================================================

void UOSCameraAutoFollowComponent::NotifyCameraInput()
{
	TimeSinceLastCameraInput = 0.0f;
}

bool UOSCameraAutoFollowComponent::IsPlayerControllingCamera() const
{
	return TimeSinceLastCameraInput < InputGracePeriod;
}

void UOSCameraAutoFollowComponent::NotifyAttackOnTarget(AActor* TargetActor)
{
	if (!TargetActor) return;

	CombatSweepTarget = TargetActor;

	// Use active config's degrees-per-attack, or a safe default
	const float DegreesPerAttack = ActiveConfig ? ActiveConfig->CombatSweepDegreesPerAttack : 15.0f;
	CombatSweepRemaining = FMath::Min(CombatSweepRemaining + DegreesPerAttack, 90.0f);
}

void UOSCameraAutoFollowComponent::ClearCombatSweep()
{
	CombatSweepTarget.Reset();
	CombatSweepRemaining = 0.0f;
}

// =============================================================================
// Zoom Override API
// =============================================================================

void UOSCameraAutoFollowComponent::ZoomIn(float TargetBoomLength, float TargetFOV, float TransitionSpeed)
{
	bZoomOverrideActive = true;
	ZoomOverrideBoomLength = TargetBoomLength;
	ZoomOverrideFOV = TargetFOV;
	ZoomOverrideTransitionSpeed = TransitionSpeed;
}

void UOSCameraAutoFollowComponent::ZoomOut(float TargetBoomLength, float TargetFOV, float TransitionSpeed)
{
	bZoomOverrideActive = true;
	ZoomOverrideBoomLength = TargetBoomLength;
	ZoomOverrideFOV = TargetFOV;
	ZoomOverrideTransitionSpeed = TransitionSpeed;
}

void UOSCameraAutoFollowComponent::ClearZoomOverride()
{
	bZoomOverrideActive = false;
}

// =============================================================================
// Tick
// =============================================================================

void UOSCameraAutoFollowComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// WRONG: CacheReferences() every Tick is redundant after first success; could cache only on pawn change / OnPossess.
	CacheReferences();
	if (!OwnerPC || !CachedPawn || !CachedBoom) return;

	// --- Advance timers ---
	TimeSinceLastCameraInput += DeltaTime;

	float MoveYaw;
	if (GetCharacterMovementYaw(MoveYaw))
	{
		TimeSinceLastMovement = 0.0f;
	}
	else
	{
		TimeSinceLastMovement += DeltaTime;
	}

	// --- Evaluate Chooser Table for current state ---
	// WRONG: Chooser evaluated every frame; RIGHT: same data-driven pattern as OSCameraModeComponent (state -> config).
	const FOSCameraStateParams StateParams = BuildStateParams();
	UOSCameraConfigDataAsset* ResolvedConfig = EvaluateChooser(StateParams);

	if (ResolvedConfig)
	{
		ActiveConfig = ResolvedConfig;
	}
	else if (FallbackConfig)
	{
		ActiveConfig = FallbackConfig;
	}

	// --- Apply framing (boom length, FOV, offsets) ---
	// RIGHT: Zoom transitions smooth regardless of player input. WRONG: ApplyConfig runs every frame even when ActiveConfig unchanged.
	ApplyConfig(DeltaTime);

	// --- Auto-behaviors: suppressed if player is controlling camera ---
	if (IsPlayerControllingCamera())
	{
		CurrentSweepSpeed = 0.0f;
		if (bDebugDraw) DrawDebugInfo();
		return;
	}

	if (!ActiveConfig)
	{
		if (bDebugDraw) DrawDebugInfo();
		return;
	}

	// RIGHT: Clear priority order (combat > locomotion > auto-follow) and player input suppresses all auto-behaviors.
	// Priority: Combat sweep > Locomotion sweep > Auto-follow
	if (ActiveConfig->bEnableCombatSweep && CombatSweepTarget.IsValid() && CombatSweepRemaining > 0.0f)
	{
		UpdateCombatSweep(DeltaTime);
	}
	else if (ActiveConfig->bEnableLocomotionSweep)
	{
		float CharMoveYaw;
		if (GetCharacterMovementYaw(CharMoveYaw))
		{
			const float CameraYaw = GetCameraYaw();
			const float MoveDelta = FMath::Abs(FRotator::NormalizeAxis(CharMoveYaw - CameraYaw));

			if (MoveDelta > ActiveConfig->LocomotionSweepMinAngle)
			{
				UpdateLocomotionSweep(DeltaTime);
			}
			else
			{
				CurrentSweepSpeed = FMath::FInterpTo(CurrentSweepSpeed, 0.0f, DeltaTime, ActiveConfig->LocomotionSweepAcceleration);
			}
		}
		else
		{
			CurrentSweepSpeed = FMath::FInterpTo(CurrentSweepSpeed, 0.0f, DeltaTime, ActiveConfig->LocomotionSweepAcceleration);

			if (ActiveConfig->bEnableAutoFollow && TimeSinceLastMovement > ActiveConfig->AutoFollowIdleDelay)
			{
				UpdateAutoFollow(DeltaTime);
			}
		}
	}
	else if (ActiveConfig->bEnableAutoFollow && TimeSinceLastMovement > ActiveConfig->AutoFollowIdleDelay)
	{
		UpdateAutoFollow(DeltaTime);
	}

	if (bDebugDraw) DrawDebugInfo();
}

// =============================================================================
// Reference Caching
// =============================================================================

void UOSCameraAutoFollowComponent::CacheReferences()
{
	// RIGHT: Same owner pattern as OSCameraModeComponent (PC owner, pawn/boom/camera from possessed pawn); only updates when pawn changes.
	if (!OwnerPC)
	{
		OwnerPC = Cast<APlayerController>(GetOwner());
		if (!OwnerPC) return;
	}

	APawn* CurrentPawn = OwnerPC->GetPawn();
	if (!CurrentPawn) return;

	if (CachedPawn != CurrentPawn)
	{
		CachedPawn = CurrentPawn;
		CachedBoom = CachedPawn->FindComponentByClass<USpringArmComponent>();
		CachedCamera = CachedPawn->FindComponentByClass<UCameraComponent>();

		if (!CachedBoom)
		{
			// WRONG: LogTemp is generic; use a dedicated log category (e.g. LogOSCameraAutoFollow).
			UE_LOG(LogTemp, Warning, TEXT("OSCameraAutoFollow: No SpringArmComponent found on pawn."));
		}
	}
}

// =============================================================================
// Chooser Evaluation
// =============================================================================

FOSCameraStateParams UOSCameraAutoFollowComponent::BuildStateParams() const
{
	// RIGHT: Uses ASC from pawn (AbilitySystemGlobals), only tags (no hardcoded state).
	FOSCameraStateParams Params;

	if (!CachedPawn) return Params;

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedPawn.Get());
	if (!ASC) return Params;

	ASC->GetOwnedGameplayTags(Params.ActiveTags);

	return Params;
}

UOSCameraConfigDataAsset* UOSCameraAutoFollowComponent::EvaluateChooser(const FOSCameraStateParams& Params) const
{
	if (!CameraChooserTable) return nullptr;

	return OSChooser::Evaluate<UOSCameraConfigDataAsset>(CameraChooserTable, Params);
}

// =============================================================================
// Config Application (Framing / Zoom)
// =============================================================================

void UOSCameraAutoFollowComponent::ApplyConfig(float DeltaTime)
{
	if (!CachedBoom || !CachedCamera) return;

	float TargetBoomLength;
	float TargetFOV;
	float TransitionSpeed;

	if (bZoomOverrideActive)
	{
		// Manual zoom override takes priority
		TargetBoomLength = ZoomOverrideBoomLength;
		TargetFOV = ZoomOverrideFOV;
		TransitionSpeed = ZoomOverrideTransitionSpeed;
	}
	else if (ActiveConfig)
	{
		TargetBoomLength = ActiveConfig->BoomLength;
		TargetFOV = ActiveConfig->CameraFOV;
		TransitionSpeed = ActiveConfig->TransitionSpeed;
	}
	else
	{
		return; // No config, nothing to apply
	}

	// Interpolate boom length
	const float CurrentBoomLength = CachedBoom->TargetArmLength;
	CachedBoom->TargetArmLength = FMath::FInterpTo(CurrentBoomLength, TargetBoomLength, DeltaTime, TransitionSpeed);

	// Interpolate FOV
	const float CurrentFOV = CachedCamera->FieldOfView;
	CachedCamera->SetFieldOfView(FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, TransitionSpeed));

	// Interpolate socket offset (height + side)
	// WRONG: Socket offset only applied when not in zoom override; during ZoomIn/Out the offset is not driven to a defined value.
	if (ActiveConfig && !bZoomOverrideActive)
	{
		const FVector CurrentOffset = CachedBoom->SocketOffset;
		const FVector TargetOffset(0.0f, ActiveConfig->CameraSideOffset, ActiveConfig->CameraHeight);
		CachedBoom->SocketOffset = FMath::VInterpTo(CurrentOffset, TargetOffset, DeltaTime, TransitionSpeed);
	}
}

// =============================================================================
// Auto-Follow Behind
// =============================================================================

void UOSCameraAutoFollowComponent::UpdateAutoFollow(float DeltaTime)
{
	if (!ActiveConfig) return;

	const float CharYaw = GetCharacterForwardYaw();
	const float Delta = GetYawDeltaToTarget(CharYaw);

	if (FMath::Abs(Delta) < ActiveConfig->AutoFollowDeadzone) return;

	const float MaxStep = ActiveConfig->AutoFollowSpeed * DeltaTime;
	const float Step = FMath::Clamp(Delta, -MaxStep, MaxStep);
	ApplyYawToController(Step);
}

// =============================================================================
// Locomotion Sweep
// =============================================================================

void UOSCameraAutoFollowComponent::UpdateLocomotionSweep(float DeltaTime)
{
	if (!ActiveConfig || !CachedPawn) return;

	// Ramp sweep speed toward config value
	CurrentSweepSpeed = FMath::FInterpTo(CurrentSweepSpeed, ActiveConfig->LocomotionSweepSpeed, DeltaTime, ActiveConfig->LocomotionSweepAcceleration);

	float MoveYaw;
	if (!GetCharacterMovementYaw(MoveYaw)) return;

	const float Delta = GetYawDeltaToTarget(MoveYaw);
	if (FMath::Abs(Delta) < 1.0f) return;

	const float MaxStep = CurrentSweepSpeed * DeltaTime;
	const float Step = FMath::Clamp(Delta, -MaxStep, MaxStep);
	ApplyYawToController(Step);
}

// =============================================================================
// Combat Soft-Lock Sweep
// =============================================================================

void UOSCameraAutoFollowComponent::UpdateCombatSweep(float DeltaTime)
{
	if (!CombatSweepTarget.IsValid() || !CachedPawn || !ActiveConfig)
	{
		ClearCombatSweep();
		return;
	}

	const FVector PawnLoc = CachedPawn->GetActorLocation();
	const FVector TargetLoc = CombatSweepTarget->GetActorLocation();
	const FVector ToTarget = (TargetLoc - PawnLoc).GetSafeNormal2D();

	if (ToTarget.IsNearlyZero())
	{
		ClearCombatSweep();
		return;
	}

	const float DesiredYaw = FMath::Atan2(ToTarget.Y, ToTarget.X) * (180.0f / PI);
	const float Delta = GetYawDeltaToTarget(DesiredYaw);

	if (FMath::Abs(Delta) < 1.0f)
	{
		CombatSweepRemaining = 0.0f;
		return;
	}

	const float MaxStep = FMath::Min(ActiveConfig->CombatSweepSpeed * DeltaTime, CombatSweepRemaining);
	const float Step = FMath::Clamp(Delta, -MaxStep, MaxStep);

	ApplyYawToController(Step);
	CombatSweepRemaining = FMath::Max(0.0f, CombatSweepRemaining - FMath::Abs(Step));
}

// =============================================================================
// Helpers
// =============================================================================

float UOSCameraAutoFollowComponent::GetYawDeltaToTarget(float TargetYaw) const
{
	return FRotator::NormalizeAxis(TargetYaw - GetCameraYaw());
}

bool UOSCameraAutoFollowComponent::GetCharacterMovementYaw(float& OutYaw) const
{
	if (!CachedPawn) return false;

	const ACharacter* Character = Cast<ACharacter>(CachedPawn.Get());
	if (!Character) return false;

	const UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	if (!CMC) return false;

	// RIGHT: Uses velocity for movement direction. WRONG: Magic number 100 (min speed squared); could be config or named constant.
	const FVector Velocity2D(CMC->Velocity.X, CMC->Velocity.Y, 0.0f);
	if (Velocity2D.SizeSquared() < 100.0f) return false;

	const FVector Dir = Velocity2D.GetSafeNormal();
	OutYaw = FMath::Atan2(Dir.Y, Dir.X) * (180.0f / PI);
	return true;
}

void UOSCameraAutoFollowComponent::ApplyYawToController(float YawDelta) const
{
	if (!OwnerPC || FMath::IsNearlyZero(YawDelta)) return;

	FRotator ControlRot = OwnerPC->GetControlRotation();
	ControlRot.Yaw += YawDelta;
	OwnerPC->SetControlRotation(ControlRot);
}

float UOSCameraAutoFollowComponent::GetCameraYaw() const
{
	return OwnerPC ? OwnerPC->GetControlRotation().Yaw : 0.0f;
}

float UOSCameraAutoFollowComponent::GetCharacterForwardYaw() const
{
	return CachedPawn ? CachedPawn->GetActorRotation().Yaw : 0.0f;
}

// =============================================================================
// Debug
// =============================================================================

void UOSCameraAutoFollowComponent::DrawDebugInfo() const
{
	// RIGHT: Debug draw guarded by !UE_BUILD_SHIPPING so it never runs in shipping builds.
#if !UE_BUILD_SHIPPING
	if (!CachedPawn || !OwnerPC) return;
	const UWorld* World = GetWorld();
	if (!World) return;

	const FVector Loc = CachedPawn->GetActorLocation() + FVector(0, 0, 150.0f);
	const float CamYaw = GetCameraYaw();
	const float CharYaw = GetCharacterForwardYaw();

	// Camera forward (cyan)
	DrawDebugDirectionalArrow(World, Loc, Loc + FRotator(0, CamYaw, 0).Vector() * 300.0f, 30.0f, FColor::Cyan, false, -1, 0, 2.0f);

	// Character forward (green)
	DrawDebugDirectionalArrow(World, Loc, Loc + FRotator(0, CharYaw, 0).Vector() * 200.0f, 25.0f, FColor::Green, false, -1, 0, 2.0f);

	// Movement direction (yellow)
	float MoveYaw;
	if (GetCharacterMovementYaw(MoveYaw))
	{
		DrawDebugDirectionalArrow(World, Loc, Loc + FRotator(0, MoveYaw, 0).Vector() * 250.0f, 25.0f, FColor::Yellow, false, -1, 0, 2.0f);
	}

	// Combat sweep target (red)
	if (CombatSweepTarget.IsValid())
	{
		DrawDebugLine(World, Loc, CombatSweepTarget->GetActorLocation() + FVector(0, 0, 90.0f), FColor::Red, false, -1, 0, 3.0f);
	}

	// Status
	const FString Behavior = IsPlayerControllingCamera() ? TEXT("PLAYER INPUT")
		: (CombatSweepTarget.IsValid() && CombatSweepRemaining > 0.0f) ? TEXT("COMBAT SWEEP")
		: (CurrentSweepSpeed > 1.0f) ? TEXT("LOCO SWEEP")
		: (ActiveConfig && ActiveConfig->bEnableAutoFollow && TimeSinceLastMovement > ActiveConfig->AutoFollowIdleDelay) ? TEXT("AUTO-FOLLOW")
		: TEXT("IDLE");

	const FString ConfigName = ActiveConfig ? ActiveConfig->GetName() : TEXT("NONE");

	const FString Status = FString::Printf(
		TEXT("Cam: %s | Config: %s | Boom: %.0f | FOV: %.0f | SweepSpd: %.1f | CombatRem: %.1f"),
		*Behavior, *ConfigName,
		CachedBoom ? CachedBoom->TargetArmLength : 0.0f,
		CachedCamera ? CachedCamera->FieldOfView : 0.0f,
		CurrentSweepSpeed, CombatSweepRemaining
	);

	DrawDebugString(World, Loc + FVector(0, 0, 50.0f), Status, nullptr, FColor::White, 0.0f, true);
#endif
}
