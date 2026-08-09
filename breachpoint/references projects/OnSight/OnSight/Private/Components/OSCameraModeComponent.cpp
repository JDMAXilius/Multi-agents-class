// Copyright Epic Games, Inc. All Rights Reserved.
// Event-driven camera: Chooser + engagement presets; no Tick. Client-only.

#include "Components/OSCameraModeComponent.h"
#include "Utilities/ChooserHelper.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/EngineTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogOSCameraMode, Log, All);

UOSCameraModeComponent::UOSCameraModeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CurrentEngagementTier = EOSEngagementTier::None;
	TimeInLowerTier = 0.0f;
}

void UOSCameraModeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwnerRole() != ROLE_AutonomousProxy)
	{
		UE_LOG(LogOSCameraMode, Verbose, TEXT("BeginPlay: skipped (owner not AutonomousProxy, role=%d)"), static_cast<int32>(GetOwnerRole()));
		return;
	}
	CacheReferences();
	if (!OwnerPC || !CachedPawn)
	{
		UE_LOG(LogOSCameraMode, Verbose, TEXT("BeginPlay: no possessed pawn yet on %s (will init on OnPawnPossessed)"), *GetNameSafe(GetOwner()));
		return;
	}
	if (!CameraModeConfig) { UE_LOG(LogOSCameraMode, Warning, TEXT("BeginPlay: no CameraModeConfig")); return; }
	RegisterListenersAndEvaluate();
	if (UWorld* W = GetWorld()) W->GetTimerManager().SetTimer(DelayedEngagementCheckTimer, this, &UOSCameraModeComponent::OnDelayedEngagementCheck, 2.0f, false);
}

void UOSCameraModeComponent::OnPawnPossessed()
{
	StopLockOnTimer();
	StopPresetBlendTimer();
	if (DelayedEngagementCheckTimer.IsValid() && GetWorld()) GetWorld()->GetTimerManager().ClearTimer(DelayedEngagementCheckTimer);
	DelayedEngagementCheckTimer.Invalidate();
	UnregisterTagListeners();
	CacheReferences();
	if (!OwnerPC || !CachedPawn || !CameraModeConfig) return;
	RegisterListenersAndEvaluate();
}

void UOSCameraModeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLockOnTimer();
	StopPresetBlendTimer();
	if (DelayedEngagementCheckTimer.IsValid() && GetWorld()) GetWorld()->GetTimerManager().ClearTimer(DelayedEngagementCheckTimer);
	DelayedEngagementCheckTimer.Invalidate();
	UnregisterTagListeners();
	Super::EndPlay(EndPlayReason);
}

void UOSCameraModeComponent::SetFocusTarget(AActor* Target)
{
	FocusTarget = Target;
	IsValid(FocusTarget) ? StartLockOnTimerIfNeeded() : StopLockOnTimer();
}

void UOSCameraModeComponent::CacheReferences()
{
	if (!OwnerPC)
	{
		OwnerPC = Cast<APlayerController>(GetOwner());
		if (!OwnerPC) return;
	}

	APawn* CurrentPawn = OwnerPC->GetPawn();
	if (!CurrentPawn)
	{
		CachedPawn = nullptr;
		SpringArm = nullptr;
		Camera = nullptr;
		ASC = nullptr;
		return;
	}

	if (CachedPawn != CurrentPawn)
	{
		CachedPawn = CurrentPawn;
		SpringArm = CachedPawn->FindComponentByClass<USpringArmComponent>();
		Camera = CachedPawn->FindComponentByClass<UCameraComponent>();
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedPawn);
	}
}

void UOSCameraModeComponent::RegisterTagListeners()
{
	if (!ASC || !CameraModeConfig) return;

	for (const FGameplayTag& Tag : CameraModeConfig->TagsToObserve)
	{
		if (!Tag.IsValid()) continue;
		FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UOSCameraModeComponent::OnObservedTagChanged);
		TagDelegateHandles.Add(Tag, Handle);
	}
}

void UOSCameraModeComponent::UnregisterTagListeners()
{
	if (ASC)
	{
		for (const auto& Pair : TagDelegateHandles)
		{
			if (Pair.Value.IsValid())
			{
				ASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
			}
		}
	}
	TagDelegateHandles.Empty();
}

void UOSCameraModeComponent::RegisterListenersAndEvaluate()
{
	RegisterTagListeners();
	ResolveAndApplyPreset(0.f);
	if (UWorld* W = GetWorld()) LastEngagementUpdateTime = W->GetTimeSeconds();
}

void UOSCameraModeComponent::OnObservedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const float DeltaTime = GetWorld() ? GetWorld()->GetTimeSeconds() - LastEngagementUpdateTime : 0.f;
	ResolveAndApplyPreset(DeltaTime);
	if (UWorld* W = GetWorld()) LastEngagementUpdateTime = W->GetTimeSeconds();
	if (CameraModeConfig && CameraModeConfig->LockOnTag.IsValid() && Tag == CameraModeConfig->LockOnTag)
		NewCount > 0 ? StartLockOnTimerIfNeeded() : StopLockOnTimer();
}

void UOSCameraModeComponent::ResolveAndApplyPreset(float DeltaTimeForEngagement)
{
	if (!CameraModeConfig) return;
	const float Radius = CameraModeConfig->EngagementScanRadius;
	const int32 ThreatCount = CountThreatsInRadius(Radius);
	UpdateEngagementTier(DeltaTimeForEngagement, ThreatCount);
	const FOSCameraModeChooserContext Context = BuildChooserContext(ThreatCount);
	UOSCameraPreset* ToApply = nullptr;
	bool bFromEngagement = false;
	if (Context.ThreatCount == 1 && CameraModeConfig->PresetOneTarget.Get()) { ToApply = CameraModeConfig->PresetOneTarget.Get(); bFromEngagement = true; }
	else if (Context.ThreatCount >= 2 && CameraModeConfig->PresetMultipleTargets.Get()) { ToApply = CameraModeConfig->PresetMultipleTargets.Get(); bFromEngagement = true; }
	else if (Context.ThreatCount == 1) { UE_LOG(LogOSCameraMode, Warning, TEXT("1 target in range but Preset One Target not set on config")); }
	else if (Context.ThreatCount >= 2) { UE_LOG(LogOSCameraMode, Warning, TEXT("%d targets in range but Preset Multiple Targets not set on config"), Context.ThreatCount); }

	if (!ToApply) { UOSCameraPreset* R = ResolvePresetFromChooser(Context); ToApply = R ? R : CameraModeConfig->FallbackPreset.Get(); }
	UE_LOG(LogOSCameraMode, Log, TEXT("Targets in range: %d"), Context.ThreatCount);
	if (!ToApply) { UE_LOG(LogOSCameraMode, Warning, TEXT("No preset to apply")); return; }
	if ((ToApply != CurrentPreset) || bFromEngagement) { CurrentPreset = ToApply; ApplyPreset(ToApply); }
}

FOSCameraModeChooserContext UOSCameraModeComponent::BuildChooserContext(int32 ThreatCount) const
{
	FOSCameraModeChooserContext Context;
	Context.ThreatCount = ThreatCount;
	if (ASC) ASC->GetOwnedGameplayTags(Context.ActiveTags);
	return Context;
}

UOSCameraPreset* UOSCameraModeComponent::ResolvePresetFromChooser(const FOSCameraModeChooserContext& Context) const
{
	if (!CameraModeConfig || !CameraModeConfig->CameraModeChooserTable) return nullptr;
	return OSChooser::Evaluate<UOSCameraPreset>(CameraModeConfig->CameraModeChooserTable, Context);
}

void UOSCameraModeComponent::ApplyPreset(UOSCameraPreset* Preset)
{
	if (!Preset || !SpringArm || !Camera) { if (Preset) UE_LOG(LogOSCameraMode, Warning, TEXT("ApplyPreset: no SpringArm/Camera")); return; }
	const bool bSmooth = CameraModeConfig && CameraModeConfig->bSmoothPresetTransitions;
	if (bSmooth)
	{
		StopPresetBlendTimer();
		if (UWorld* W = GetWorld()) W->GetTimerManager().SetTimer(PresetBlendTimer, this, &UOSCameraModeComponent::OnPresetBlendTick, FMath::Max(0.001f, CameraModeConfig->CameraTimerInterval), true);
		return;
	}
	const FVector ArmLoc = Preset->CameraArmTransform.GetLocation();
	SpringArm->TargetArmLength = ArmLoc.X;
	SpringArm->SocketOffset = FVector(0.f, ArmLoc.Y, ArmLoc.Z);
	SpringArm->SetRelativeRotation(Preset->CameraArmTransform.Rotator());
	SpringArm->bUsePawnControlRotation = Preset->bUsePawnControlRotation;
	SpringArm->bEnableCameraLag = Preset->bEnableCameraLag;
	SpringArm->CameraLagSpeed = Preset->CameraLagSpeed;
	SpringArm->bEnableCameraRotationLag = Preset->bEnableCameraRotationLag;
	SpringArm->CameraRotationLagSpeed = Preset->CameraRotationLagSpeed;
	Camera->SetFieldOfView(Preset->FieldOfView);
}

void UOSCameraModeComponent::OnPresetBlendTick()
{
	if (!CurrentPreset || !CameraModeConfig || !CameraModeConfig->bSmoothPresetTransitions) { StopPresetBlendTimer(); return; }
	const float Dt = CameraModeConfig->CameraTimerInterval;
	const float Speed = 1.f / FMath::Max(0.01f, CurrentPreset->BlendTimeSeconds);
	bool bDone = true;
	if (SpringArm)
	{
		const FVector TL = CurrentPreset->CameraArmTransform.GetLocation();
		const FVector Toff(0.f, TL.Y, TL.Z);
		const FRotator TRot = CurrentPreset->CameraArmTransform.Rotator();
		SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TL.X, Dt, Speed);
		SpringArm->SocketOffset = FMath::VInterpTo(SpringArm->SocketOffset, Toff, Dt, Speed);
		SpringArm->SetRelativeRotation(FMath::RInterpTo(SpringArm->GetRelativeRotation(), TRot, Dt, Speed));
		SpringArm->bUsePawnControlRotation = CurrentPreset->bUsePawnControlRotation;
		SpringArm->bEnableCameraLag = CurrentPreset->bEnableCameraLag;
		SpringArm->CameraLagSpeed = FMath::FInterpTo(SpringArm->CameraLagSpeed, CurrentPreset->CameraLagSpeed, Dt, Speed);
		SpringArm->bEnableCameraRotationLag = CurrentPreset->bEnableCameraRotationLag;
		SpringArm->CameraRotationLagSpeed = FMath::FInterpTo(SpringArm->CameraRotationLagSpeed, CurrentPreset->CameraRotationLagSpeed, Dt, Speed);
		bDone = FMath::Abs(SpringArm->TargetArmLength - TL.X) <= 0.5f && FVector::Dist(SpringArm->SocketOffset, Toff) <= 0.5f && SpringArm->GetRelativeRotation().Equals(TRot, 0.5f) && FMath::Abs(SpringArm->CameraRotationLagSpeed - CurrentPreset->CameraRotationLagSpeed) <= 0.1f;
	}
	if (Camera)
	{
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, CurrentPreset->FieldOfView, Dt, Speed));
		bDone = bDone && FMath::Abs(Camera->FieldOfView - CurrentPreset->FieldOfView) <= 0.5f;
	}
	if (bDone) StopPresetBlendTimer();
}

void UOSCameraModeComponent::StopPresetBlendTimer()
{
	if (PresetBlendTimer.IsValid() && GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PresetBlendTimer);
	PresetBlendTimer.Invalidate();
}

void UOSCameraModeComponent::UpdateLockOnCamera(float DeltaTime)
{
	if (!CameraModeConfig || !CameraModeConfig->bEnableLockOnTracking || !ASC || !CameraModeConfig->LockOnTag.IsValid() || !ASC->HasMatchingGameplayTag(CameraModeConfig->LockOnTag) || !IsValid(FocusTarget) || !OwnerPC) return;
	const FRotator TR = GetLockOnTargetRotation(FocusTarget);
	OwnerPC->SetControlRotation(FMath::RInterpTo(OwnerPC->GetControlRotation(), TR, DeltaTime, CameraModeConfig->LockOnRotationSpeed));
}

FRotator UOSCameraModeComponent::GetLockOnTargetRotation(AActor* Target) const
{
	if (!Target) return FRotator::ZeroRotator;
	FVector Loc = Target->GetActorLocation();
	Loc.Z += CameraModeConfig->LockOnVerticalOffset;
	return (Loc - (CachedPawn ? CachedPawn->GetActorLocation() : GetOwner()->GetActorLocation())).Rotation();
}

void UOSCameraModeComponent::StartLockOnTimerIfNeeded()
{
	if (!CameraModeConfig || !CameraModeConfig->bEnableLockOnTracking || !CameraModeConfig->LockOnTag.IsValid() || !ASC || !ASC->HasMatchingGameplayTag(CameraModeConfig->LockOnTag) || !IsValid(FocusTarget)) return;
	if (UWorld* W = GetWorld(); W && !LockOnUpdateTimer.IsValid()) W->GetTimerManager().SetTimer(LockOnUpdateTimer, this, &UOSCameraModeComponent::OnLockOnTimerTick, FMath::Max(0.001f, CameraModeConfig->CameraTimerInterval), true);
}

void UOSCameraModeComponent::OnLockOnTimerTick()
{
	UpdateLockOnCamera(CameraModeConfig->CameraTimerInterval);
}

void UOSCameraModeComponent::StopLockOnTimer()
{
	if (LockOnUpdateTimer.IsValid() && GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LockOnUpdateTimer);
	LockOnUpdateTimer.Invalidate();
}

void UOSCameraModeComponent::OnDelayedEngagementCheck()
{
	DelayedEngagementCheckTimer.Invalidate();
	if (!CameraModeConfig) return;
	ResolveAndApplyPreset(0.f);
	if (UWorld* W = GetWorld()) LastEngagementUpdateTime = W->GetTimeSeconds();
}

void UOSCameraModeComponent::UpdateEngagementTier(float DeltaTime, int32 ThreatCount)
{
	if (!CameraModeConfig) return;
	const EOSEngagementTier NewTier = ComputeEngagementTierFromCount(ThreatCount);
	if (NewTier < CurrentEngagementTier) { TimeInLowerTier += DeltaTime; if (TimeInLowerTier >= CameraModeConfig->TierDowngradeDelay) { CurrentEngagementTier = NewTier; TimeInLowerTier = 0.f; } }
	else { CurrentEngagementTier = NewTier; TimeInLowerTier = 0.f; }
}

EOSEngagementTier UOSCameraModeComponent::ComputeEngagementTierFromCount(int32 ThreatCount) const
{
	if (!ASC || !CameraModeConfig) return EOSEngagementTier::None;
	const bool bInCombat = CameraModeConfig->CombatTagsForEngagement.ContainsByPredicate([this](const FGameplayTag& Tag) { return Tag.IsValid() && ASC->HasMatchingGameplayTag(Tag); });
	if (!bInCombat) return EOSEngagementTier::None;
	return ThreatCount >= 2 ? EOSEngagementTier::Multiple : (ThreatCount == 1 ? EOSEngagementTier::Single : EOSEngagementTier::None);
}

int32 UOSCameraModeComponent::CountThreatsInRadius(float Radius) const
{
	AActor* Center = CachedPawn ? CachedPawn.Get() : GetOwner();
	if (!GetWorld() || !Center) return 0;
	TArray<AActor*> Overlaps;
	TArray<TEnumAsByte<EObjectTypeQuery>> Types; Types.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	TArray<AActor*> Ignore; Ignore.Add(Center);
	UKismetSystemLibrary::SphereOverlapActors(GetWorld(), Center->GetActorLocation(), Radius, Types, nullptr, Ignore, Overlaps);
	const TArray<FGameplayTag>& Excl = CameraModeConfig ? CameraModeConfig->TagsThatExcludeFromThreatCount : TArray<FGameplayTag>();
	int32 N = 0;
	for (AActor* A : Overlaps)
	{
		if (A == Center) continue;
		IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(A);
		if (!ASI) continue;
		UAbilitySystemComponent* OASC = ASI->GetAbilitySystemComponent();
		if (!OASC) continue;
		bool bSkip = false;
		for (const FGameplayTag& Et : Excl) { if (Et.IsValid() && OASC->HasMatchingGameplayTag(Et)) { bSkip = true; break; } }
		if (!bSkip) N++;
	}
	return N;
}
