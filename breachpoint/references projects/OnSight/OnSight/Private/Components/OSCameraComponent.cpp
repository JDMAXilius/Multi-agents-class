// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/OSCameraComponent.h"
#include "Utilities/ChooserHelper.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/OSPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

// =============================================================================
// Init
// =============================================================================

UOSCameraComponent::UOSCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UOSCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPC = Cast<APlayerController>(GetOwner());
	if (!ensureMsgf(OwnerPC, TEXT("OSCameraComponent must be attached to a PlayerController.")))
	{
		return;
	}

	OwnerPC->OnPossessedPawnChanged.AddDynamic(this, &UOSCameraComponent::CacheReferences);

	BindToPlayerStateGASReady();
	if (APawn* CurrentPawn = OwnerPC->GetPawn())
	{
		CacheReferences(nullptr, CurrentPawn);
	}
}

void UOSCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PSBindTimer);
	}
	if (CachedPlayerState)
	{
		CachedPlayerState->OnGASReady.RemoveDynamic(this, &UOSCameraComponent::OnPlayerStateGASReady);
		CachedPlayerState = nullptr;
	}
	UnregisterTagListeners();
	if (OwnerPC)
	{
		OwnerPC->OnPossessedPawnChanged.RemoveDynamic(this, &UOSCameraComponent::CacheReferences);
	}
	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// Tick — blend only (enabled in BeginBlendTo, disabled when blend completes)
// =============================================================================

void UOSCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickBlend(DeltaTime);
}

// =============================================================================
// PlayerState GAS — bind once; ASC + tag listeners via ApplyASC (delegate or immediate)
// =============================================================================

void UOSCameraComponent::ApplyASC(UAbilitySystemComponent* NewASC)
{
	if (!NewASC) { return; }
	ASC = NewASC;
	RegisterTagListeners();
}

void UOSCameraComponent::BindToPlayerStateGASReady()
{
	if (!OwnerPC || CachedPlayerState) { return; }

	AOSPlayerState* PS = OwnerPC->GetPlayerState<AOSPlayerState>();
	if (!PS)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(PSBindTimer, this, &UOSCameraComponent::BindToPlayerStateGASReady, 0.1f, false);
		}
		return;
	}

	CachedPlayerState = PS;
	PS->OnGASReady.AddDynamic(this, &UOSCameraComponent::OnPlayerStateGASReady);
	ApplyASC(PS->GetAbilitySystemComponent());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PSBindTimer);
	}
}

void UOSCameraComponent::OnPlayerStateGASReady(UAbilitySystemComponent* ReadyASC)
{
	ApplyASC(ReadyASC);
}

// =============================================================================
// Possession — pawn/SpringArm/Camera only; ASC from OnGASReady
// =============================================================================

void UOSCameraComponent::CacheReferences(APawn* OldPawn, APawn* NewPawn)
{
	BindToPlayerStateGASReady();

	CachedPawn = NewPawn;
	SpringArm  = nullptr;
	Camera     = nullptr;
	// ASC unchanged — same PlayerState across respawn; tag listeners stay valid

	if (!NewPawn) { return; }

	SpringArm = NewPawn->FindComponentByClass<USpringArmComponent>();
	Camera    = NewPawn->FindComponentByClass<UCameraComponent>();

	if (!SpringArm) { UE_LOG(LogOSCamera, Warning, TEXT("OSCameraComponent: No USpringArmComponent on pawn '%s'."), *GetNameSafe(NewPawn)); }
	if (!Camera)    { UE_LOG(LogOSCamera, Warning, TEXT("OSCameraComponent: No UCameraComponent on pawn '%s'."),    *GetNameSafe(NewPawn)); }

	if (DefaultPreset && SpringArm && Camera)
	{
		SnapToPreset(DefaultPreset);
		TargetPreset = DefaultPreset;
	}
}

// =============================================================================
// Tag listeners
// =============================================================================

void UOSCameraComponent::RegisterTagListeners()
{
	if (!ASC || TagsToObserve.IsEmpty()) { return; }

	for (const FGameplayTag& Tag : TagsToObserve)
	{
		if (!Tag.IsValid()) { continue; }

		FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UOSCameraComponent::OnTagChanged);
		TagHandles.Add(Tag, Handle);
	}
}

void UOSCameraComponent::UnregisterTagListeners()
{
	if (ASC)
	{
		for (const auto& Pair : TagHandles)
		{
			if (Pair.Value.IsValid())
			{
				ASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
			}
		}
	}
	TagHandles.Empty();
}

// =============================================================================
// Tag change — single entry point for all state transitions
// =============================================================================

void UOSCameraComponent::OnTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	UOSCameraConfigDataAsset* Resolved = (NewCount > 0) ? ResolvePreset() : DefaultPreset.Get();

	if (!Resolved)
	{
		UE_LOG(LogOSCamera, Warning, TEXT("OSCameraComponent: No preset for tag '%s' (Count=%d). DefaultPreset may be unset."), *Tag.ToString(), NewCount);
		return;
	}

	if (Resolved == TargetPreset.Get() && !bIsBlending) { return; }

	BeginBlendTo(Resolved);
}

// =============================================================================
// Chooser evaluation
// =============================================================================

UOSCameraConfigDataAsset* UOSCameraComponent::ResolvePreset() const
{
	if (!CameraChooserTable) { return DefaultPreset.Get(); }

	FOSCameraStateParams Params;
	if (ASC) { ASC->GetOwnedGameplayTags(Params.ActiveTags); }

	UOSCameraConfigDataAsset* Result = OSChooser::Evaluate<UOSCameraConfigDataAsset>(CameraChooserTable, Params);
	return Result ? Result : DefaultPreset.Get();
}

// =============================================================================
// Blend — begin
// =============================================================================

void UOSCameraComponent::BeginBlendTo(const UOSCameraConfigDataAsset* Preset)
{
	if (!Preset) { return; }

	// Snapshot current live values as the blend-from origin.
	// If already blending, we snapshot from the in-progress interpolated state
	// so the transition is always smooth regardless of interruption.
	if (SpringArm)
	{
		BlendFromArmLength        = SpringArm->TargetArmLength;
		BlendFromHeightOffset     = SpringArm->SocketOffset.Z;
		BlendFromSideOffset       = SpringArm->SocketOffset.Y;
		BlendFromPitchOffset      = SpringArm->GetRelativeRotation().Pitch;
		BlendFromLagSpeed         = SpringArm->CameraLagSpeed;
		BlendFromRotationLagSpeed = SpringArm->CameraRotationLagSpeed;
	}
	if (Camera)
	{
		BlendFromFOV = Camera->FieldOfView;
	}

	TargetPreset  = Preset;
	BlendElapsed  = 0.f;
	BlendDuration = FMath::Max(0.01f, Preset->BlendTime);
	bIsBlending   = true;

	SetComponentTickEnabled(true);
}

// =============================================================================
// Blend — tick update (isolated callsite)
// =============================================================================

void UOSCameraComponent::TickBlend(float DeltaTime)
{
	if (!bIsBlending || !TargetPreset) { return; }

	BlendElapsed = FMath::Min(BlendElapsed + DeltaTime, BlendDuration);

	// Normalized time [0..1]
	const float NormalizedTime = BlendElapsed / BlendDuration;

	// Sample curve if set, otherwise linear.
	float Alpha = NormalizedTime;
	if (TargetPreset && TargetPreset->BlendCurve)
	{
		Alpha = FMath::Clamp(TargetPreset->BlendCurve->GetFloatValue(NormalizedTime), 0.f, 1.f);
	}

	ApplyBlendAlpha(Alpha);

	if (BlendElapsed >= BlendDuration)
	{
		SnapToPreset(TargetPreset.Get());
		bIsBlending = false;
		SetComponentTickEnabled(false);
	}
}

// =============================================================================
// Apply — lerped alpha
// =============================================================================

void UOSCameraComponent::ApplyBlendAlpha(float Alpha) const
{
	if (!TargetPreset) { return; }

	if (SpringArm)
	{
		SpringArm->TargetArmLength        = FMath::Lerp(BlendFromArmLength,        TargetPreset->BoomLength,            Alpha);
		SpringArm->SocketOffset.Z         = FMath::Lerp(BlendFromHeightOffset,     TargetPreset->CameraHeight,          Alpha);
		SpringArm->SocketOffset.Y         = FMath::Lerp(BlendFromSideOffset,       TargetPreset->CameraSideOffset,      Alpha);
		SpringArm->CameraLagSpeed         = FMath::Lerp(BlendFromLagSpeed,         TargetPreset->CameraLagSpeed,        Alpha);
		SpringArm->CameraRotationLagSpeed = FMath::Lerp(BlendFromRotationLagSpeed, TargetPreset->CameraRotationLagSpeed, Alpha);

		FRotator ArmRot = SpringArm->GetRelativeRotation();
		ArmRot.Pitch = FMath::Lerp(BlendFromPitchOffset, TargetPreset->CameraPitchOffset, Alpha);
		SpringArm->SetRelativeRotation(ArmRot);
	}

	if (Camera)
	{
		Camera->SetFieldOfView(FMath::Lerp(BlendFromFOV, TargetPreset->CameraFOV, Alpha));
	}
}

// =============================================================================
// Apply — snap (no interpolation, used on init and blend completion)
// =============================================================================

void UOSCameraComponent::SnapToPreset(const UOSCameraConfigDataAsset* Preset) const
{
	if (!Preset) { return; }

	if (SpringArm)
	{
		SpringArm->TargetArmLength        = Preset->BoomLength;
		SpringArm->SocketOffset           = FVector(0.f, Preset->CameraSideOffset, Preset->CameraHeight);
		SpringArm->CameraLagSpeed         = Preset->CameraLagSpeed;
		SpringArm->CameraRotationLagSpeed = Preset->CameraRotationLagSpeed;

		FRotator ArmRot = SpringArm->GetRelativeRotation();
		ArmRot.Pitch = Preset->CameraPitchOffset;
		SpringArm->SetRelativeRotation(ArmRot);
	}

	if (Camera)
	{
		Camera->SetFieldOfView(Preset->CameraFOV);
	}
}
