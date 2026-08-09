#include "GAS/Abilities/GA_OSAttack.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/OSCharacter.h"
#include "Components/OSCharacterMovementComponent.h"
#include "Data/OSGameplayTags.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"
#include "GameFramework/Character.h"
#include "OSLogCategories.h"

/** Fire GameplayEvent.UI.TargetChanged so the HUD can react without polling or ability-specific delegates. */
static void BroadcastTargetChanged(UGameplayAbility* Ability, AActor* NewTarget)
{
	if (UAbilitySystemComponent* ASC = Ability->GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayEventData Payload;
		Payload.Target = NewTarget;
		ASC->HandleGameplayEvent(FOSGameplayTags::Get().UI_TargetChanged, &Payload);
	}
}

UGA_OSAttack::UGA_OSAttack()
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	ActivationBlockedTags.AddTag(Tags.IsMantling);
	ActivationBlockedTags.AddTag(Tags.IsStunned);
	ActivationBlockedTags.AddTag(Tags.IsHitReacting);
	ActivationBlockedTags.AddTag(Tags.IsRecoiling);
	ActivationBlockedTags.AddTag(Tags.IsKnockedDown);
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsBeingGrabbed);
	ActivationBlockedTags.AddTag(Tags.State_IsStunLock);
	// Block attacking during dodge i-frames — prevents players from cancelling their own dodge
	// defense by starting an attack. GA_MeleeAttackCombo has an explicit opposite pattern
	// (CancelAbilitiesWithTag on Dodge) which is intentional for the combo system, so this fix
	// only applies to GA_OSAttack-derived abilities that don't explicitly opt into dodge cancel.
	ActivationBlockedTags.AddTag(Tags.IsDodging);
}

FName UGA_OSAttack::GetWarpTargetName() const
{
	// INTENTIONAL: Per-hit indexed naming (e.g. "AttackTarget_0", "AttackTarget_2").
	// 1. Prevents stale modifiers from previous hits reading the next hit's target —
	//    engine auto-disables modifiers whose named target disappears (URootMotionModifier_Warp::Update).
	// 2. Sidesteps UE-173786: montage section jumps don't re-trigger OnBecomeRelevant,
	//    so a reused name would leave the modifier reading stale cached data. Unique names
	//    per hit force fresh modifier creation.
	// Single-hit attacks return 0 from GetWarpHitIndex() → "AttackTarget_0".
	// Combo attacks override GetWarpHitIndex() to return CurrentComboIndex.
	return FName(*FString::Printf(TEXT("%s_%d"), *WarpTargetName.ToString(), GetWarpHitIndex()));
}

void UGA_OSAttack::ClientRPC_SyncWarpState_Implementation(
	const FVector& InBlendedDirection, bool bInHasTarget, int32 InWarpHitIndex)
{
	// Reject stale RPCs: if the client has already predicted ahead to a later combo hit,
	// applying this older hit's direction would corrupt the current warp and cause teleportation.
	// Under latency, the server's hit-1 RPC can arrive while the client is already on hit 2.
	if (InWarpHitIndex < GetWarpHitIndex())
	{
		UE_LOG(LogOSAttack, Verbose,
			TEXT("ClientRPC_SyncWarpState: rejected stale RPC (server hit %d, client hit %d)"),
			InWarpHitIndex, GetWarpHitIndex());
		return;
	}

	StoredBlendedDirection2D = InBlendedDirection;
	bWarpHasTarget = bInHasTarget;
}

void UGA_OSAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearPhaseState();

	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		ClearWarpTarget(Character);
	}

	if (Target.Get())
	{
		BroadcastTargetChanged(this, nullptr);
	}

	Target = nullptr;
	bWarpHasTarget = false;
	CachedMWC = nullptr;

	if (UAbilitySystemComponent* A = ASC())
	{
		A->SetReplicatedLooseGameplayTagCount(FOSGameplayTags::Get().IsLockedOn, 0);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (ACharacter* CharacterAfterEnd = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (CharacterAfterEnd->HasAuthority())
		{
			if (UOSCharacterMovementComponent* OSMove =
				Cast<UOSCharacterMovementComponent>(CharacterAfterEnd->GetCharacterMovement()))
			{
				OSMove->OnServerPostAttackMovementResume();
			}
		}
	}
}

void UGA_OSAttack::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	ClearPhaseState();

	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		ClearWarpTarget(Character);
	}
	Target = nullptr;
	bWarpHasTarget = false;
	CachedMWC = nullptr;

	// IsLockedOn cleanup is handled by EndAbility (which always runs before OnRemoveAbility
	// for active abilities). If the ability was never activated, IsLockedOn was never set,
	// so no cleanup needed here. Removing the redundant ASC() call eliminates the null
	// dereference risk during PlayerState teardown on disconnect.

	Super::OnRemoveAbility(ActorInfo, Spec);
}

// ========================================
// TARGET RESOLUTION & WARP
// ========================================

void UGA_OSAttack::FillSoftTargetParams(FOSAttackDirectionParams& OutParams) const
{
	OutParams.ConeAngleDegrees = SoftTargetConeAngle;
	OutParams.MaxRange = SoftTargetMaxRange;
	OutParams.ExcludeTags = SoftTargetExcludeTags;
	OutParams.BlendAlgorithm = BlendAlgorithm;
	OutParams.InputInfluence = SoftTargetInputInfluence;
	OutParams.MagneticStickiness = MagneticStickiness;
	OutParams.MagneticBreakawayAngle = MagneticBreakawayAngle;
	OutParams.DeadZoneAngle = DeadZoneAngle;
	OutParams.FullControlAngle = FullControlAngle;
	OutParams.BlendCurveExponent = BlendCurveExponent;
	OutParams.MaxDeviationAngle = MaxDeviationAngle;
	OutParams.ConeInputWeight = ConeInputWeight;
}

void UGA_OSAttack::ResolveAttackTarget(UAnimMontage* Montage)
{
	AActor* OldTarget = Target.Get();

	UAbilitySystemComponent* const AbilityComp = ASC();
	const FGameplayTag& LockedOnTag = FOSGameplayTags::Get().IsLockedOn;

	AOSCharacter* Char = Avatar();
	if (!Char || UOSCombatBlueprintLibrary::IsIncapacitated(Char))
	{
		Target = nullptr;
		StoredBlendedDirection2D = FVector::ZeroVector;
		StoredTriggerEventData.Target = nullptr;
		if (AbilityComp)
		{
			AbilityComp->SetReplicatedLooseGameplayTagCount(LockedOnTag, 0);
		}
		if (OldTarget) { BroadcastTargetChanged(this, nullptr); }
		return;
	}

	// Combo persistence: stay locked to existing target if valid and in range.
	if (IsValid(Target.Get()) && !UOSCombatBlueprintLibrary::IsIncapacitated(Target.Get()))
	{
		const float DistSq = FVector::DistSquared(Char->GetActorLocation(), Target->GetActorLocation());
		if (DistSq <= FMath::Square(SoftTargetMaxRange))
		{
			// FIX: Ensure direction is always valid even during combo updates.
			const FVector ToTarget = (Target->GetActorLocation() - Char->GetActorLocation()).GetSafeNormal2D();
			StoredBlendedDirection2D = ToTarget.IsNearlyZero() ? Char->GetActorForwardVector().GetSafeNormal2D() : ToTarget;
			StoredTriggerEventData.Target = Target.Get();
			if (AbilityComp)
			{
				AbilityComp->AddReplicatedLooseGameplayTag(LockedOnTag);
			}
			return;
		}
	}

	// No persisted target — resolve fresh.
	Target = nullptr;
	StoredBlendedDirection2D = FVector::ZeroVector;
	StoredTriggerEventData.Target = nullptr;

	if (!bSoftTargeting)
	{
		StoredBlendedDirection2D = UOSCombatBlueprintLibrary::GetLastMovementDirection2D(Char);
		return;
	}

	FOSAttackDirectionParams Params;
	FillSoftTargetParams(Params);

	FVector FinalDir2D;
	AOSCharacter* ResolvedTarget = nullptr;
	bool bFound = UOSCombatBlueprintLibrary::ResolveAttackDirectionInCone(Char, Params, FinalDir2D, ResolvedTarget);

	// AI fallback: retry with full-sphere search.
	if (!bFound && !Char->IsPlayerControlled())
	{
		FOSAttackDirectionParams WideParams = Params;
		WideParams.ConeAngleDegrees = 360.f;
		bFound = UOSCombatBlueprintLibrary::ResolveAttackDirectionInCone(Char, WideParams, FinalDir2D, ResolvedTarget);
	}

	Target = ResolvedTarget;
	StoredTriggerEventData.Target = Target.Get();
	StoredBlendedDirection2D = bFound ? FinalDir2D : UOSCombatBlueprintLibrary::GetLastMovementDirection2D(Char);

	// Update IsLockedOn tag based on whether we have a valid soft-lock target.
	// Previously set inside InjectWarpTargets (MW-specific); moved here because it's
	// a targeting concern, not a drive/MW concern.
	if (AbilityComp)
	{
		if (IsValid(Target.Get()))
		{
			AbilityComp->AddReplicatedLooseGameplayTag(LockedOnTag);
		}
		else
		{
			AbilityComp->SetReplicatedLooseGameplayTagCount(LockedOnTag, 0);
		}
	}

	if (bFound && !Char->IsPlayerControlled())
	{
		Char->SetActorRotation(FRotator(0.f, FinalDir2D.Rotation().Yaw, 0.f));
	}

	if (Target.Get() != OldTarget)
	{
		BroadcastTargetChanged(this, Target.Get());
	}
}

void UGA_OSAttack::InjectWarpTargets(ACharacter* Character)
{
	if (!IsValid(Character)) return;

	UMotionWarpingComponent* MWC = Character->FindComponentByClass<UMotionWarpingComponent>();
	if (!MWC) return;

#if !UE_BUILD_SHIPPING
	// Diagnostic: log modifier states at injection time. Previous-hit modifiers are expected
	// here — they're cleaned up on the next animation tick (Update detects missing target or
	// animation position jump → Disabled/MarkedForRemoval), before ProcessRootMotion runs.
	const TArray<URootMotionModifier*>& PreInjectMods = MWC->GetModifiers();
	for (const URootMotionModifier* Mod : PreInjectMods)
	{
		const URootMotionModifier_Warp* WarpMod = Cast<URootMotionModifier_Warp>(Mod);
		const TCHAR* StateStr = TEXT("Unknown");
		switch (Mod->GetState())
		{
		case ERootMotionModifierState::Waiting:          StateStr = TEXT("Waiting"); break;
		case ERootMotionModifierState::Active:            StateStr = TEXT("Active"); break;
		case ERootMotionModifierState::MarkedForRemoval:  StateStr = TEXT("MarkedForRemoval"); break;
		case ERootMotionModifierState::Disabled:          StateStr = TEXT("Disabled"); break;
		}
		UE_LOG(LogOSAttack, Log,
			TEXT("[%s] InjectWarpTargets: existing modifier state=%s anim=%s [%.3f-%.3f] warpName=%s | owner=%s"),
			Character->HasAuthority() ? TEXT("SRV") : TEXT("CLT"),
			StateStr,
			*GetNameSafe(Mod->GetAnimation()),
			Mod->StartTime, Mod->EndTime,
			WarpMod ? *WarpMod->WarpTargetName.ToString() : TEXT("N/A"),
			*Character->GetName());
	}
#endif

	CachedMWC = MWC;
	bWarpHasTarget = IsValid(Target.Get());

	const FVector OwnerLoc = Character->GetActorLocation();

	if (bWarpHasTarget)
	{
		const FVector TargetLoc = Target->GetActorLocation();

		// STABILITY FIX: Lock the approach vector at the start of the lunge.
		// We lunge relative to the target's location, not the owner's shifting location.
		FVector AttackDir = FVector(StoredBlendedDirection2D.X, StoredBlendedDirection2D.Y, 0.f);
		if (AttackDir.IsNearlyZero())
		{
			AttackDir = (TargetLoc - OwnerLoc).GetSafeNormal2D();
			if (AttackDir.IsNearlyZero()) AttackDir = Character->GetActorForwardVector().GetSafeNormal2D();

			// AUTHORITATIVE LOCK: Save the calculated fallback so RefreshWarpTarget
			// uses this exact same vector for the entire duration of the lunge.
			StoredBlendedDirection2D = AttackDir;
		}

		const float CapsuleDist = UOSCombatBlueprintLibrary::GetCapsuleStopDistance(Character, Cast<ACharacter>(Target.Get()));
		const float StopDist = FMath::Max(CapsuleDist + 5.f, OptimalMeleeDistance);

		// If already within strike range, stay put (rotation-only warp).
		// SkewWarp zeros out translation when target == current position.
		const float CurrentDist = FVector::Dist2D(OwnerLoc, TargetLoc);
		FVector WarpLoc;
		if (CurrentDist <= StopDist)
		{
			WarpLoc = OwnerLoc;
		}
		else
		{
			WarpLoc = TargetLoc - AttackDir * StopDist;
		}
		WarpLoc.Z = OwnerLoc.Z;

		const FRotator WarpRot(0.f, AttackDir.Rotation().Yaw, 0.f);
		const FName ActiveName = GetWarpTargetName();
		const FTransform WarpXf(WarpRot, WarpLoc);
		MWC->AddOrUpdateWarpTargetFromTransform(ActiveName, WarpXf);

		// Replicate warp target to simulated proxies for root motion matching
		if (HasAuthority(&CurrentActivationInfo))
		{
			if (AOSCharacter* OSChar = Cast<AOSCharacter>(Character))
			{
				OSChar->SetReplicatedWarpTarget(ActiveName, WarpXf);
			}
		}

#if !UE_BUILD_SHIPPING
		DebugWarpPathStart = OwnerLoc;
		DebugWarpPathEnd = WarpLoc;
		DebugLastTrackedLoc = OwnerLoc;
#endif
	}
	else
	{
		FVector Dir2D(StoredBlendedDirection2D.X, StoredBlendedDirection2D.Y, 0.f);
		if (Dir2D.IsNearlyZero())
		{
			Dir2D = UOSCombatBlueprintLibrary::GetAimDirection2D(Character);
			Dir2D.Z = 0.f;
		}
		if (Dir2D.Normalize())
		{
			const FRotator WarpRot(0.f, Dir2D.Rotation().Yaw, 0.f);
			FVector WarpLoc = OwnerLoc + Dir2D * DirectionalFallbackWarpDistance;
			WarpLoc.Z = OwnerLoc.Z;
			const FName ActiveName = GetWarpTargetName();
			const FTransform WarpXf(WarpRot, WarpLoc);
			MWC->AddOrUpdateWarpTargetFromTransform(ActiveName, WarpXf);

			// Replicate warp target to simulated proxies for root motion matching
			if (HasAuthority(&CurrentActivationInfo))
			{
				if (AOSCharacter* OSChar = Cast<AOSCharacter>(Character))
				{
					OSChar->SetReplicatedWarpTarget(ActiveName, WarpXf);
				}
			}

#if !UE_BUILD_SHIPPING
			DebugWarpPathStart = OwnerLoc;
			DebugWarpPathEnd = WarpLoc;
			DebugLastTrackedLoc = OwnerLoc;
#endif
		}
	}

	// Sync authoritative warp state to the owning client. With LocalPredicted, both machines
	// run ResolveAttackTarget and InjectWarpTargets independently — they usually agree, but
	// latency can cause the client to resolve a different target or direction than the server.
	// This RPC ensures the client's instance matches the server's authoritative state.
	if (HasAuthority(&CurrentActivationInfo))
	{
		ClientRPC_SyncWarpState(StoredBlendedDirection2D, bWarpHasTarget, GetWarpHitIndex());
	}
}

FVector UGA_OSAttack::BlendDirectionForWarp(const FVector& BaseDir, const FVector& InputDir) const
{
	FOSAttackDirectionParams Params;
	FillSoftTargetParams(Params);

	const FVector Result = UOSCombatBlueprintLibrary::DispatchBlendAlgorithm(BaseDir, InputDir, Params);
	return Result.IsNearlyZero() ? BaseDir : Result;
}

// ========================================
// WARP TARGET HELPERS
// ========================================

void UGA_OSAttack::ComputeWarpPositionWithTarget(const ACharacter* Character, const FVector& OwnerLoc, FVector& OutWarpLoc) const
{
	const FVector TargetLoc = Target->GetActorLocation();

	FVector AttackDir = FVector(StoredBlendedDirection2D.X, StoredBlendedDirection2D.Y, 0.f);
	if (AttackDir.IsNearlyZero())
	{
		AttackDir = (TargetLoc - OwnerLoc).GetSafeNormal2D();
		if (AttackDir.IsNearlyZero()) AttackDir = Character->GetActorForwardVector().GetSafeNormal2D();
	}

	const float CapsuleDist = UOSCombatBlueprintLibrary::GetCapsuleStopDistance(Character, Cast<ACharacter>(Target.Get()));
	const float StopDist = FMath::Max(CapsuleDist + 5.f, OptimalMeleeDistance);

	const float CurrentDist = FVector::Dist2D(OwnerLoc, TargetLoc);
	OutWarpLoc = (CurrentDist <= StopDist) ? OwnerLoc : TargetLoc - AttackDir * StopDist;
	OutWarpLoc.Z = OwnerLoc.Z;
}

bool UGA_OSAttack::ComputeWarpRotation(const ACharacter* Character, const FVector& InputDir, FVector& OutFaceDir) const
{
	FVector BaseDir(StoredBlendedDirection2D.X, StoredBlendedDirection2D.Y, 0.f);
	if (BaseDir.IsNearlyZero())
	{
		BaseDir = UOSCombatBlueprintLibrary::GetAimDirection2D(Character);
	}
	BaseDir.Z = 0.f;
	if (!BaseDir.Normalize()) return false;

	OutFaceDir = BlendDirectionForWarp(BaseDir, InputDir);
	return true;
}

FVector UGA_OSAttack::ApplyRotationDamping(const UMotionWarpingComponent* MWC, const FVector& DesiredDir, float DeltaTime) const
{
	if (WarpRotationSpeedDegPerSec <= KINDA_SMALL_NUMBER || DeltaTime <= 0.f)
	{
		return DesiredDir;
	}

	const FMotionWarpingTarget* Existing = MWC->FindWarpTarget(GetWarpTargetName());
	if (!Existing) return DesiredDir;

	const FVector CurrentFaceDir = Existing->GetRotation().GetForwardVector().GetSafeNormal2D();
	if (CurrentFaceDir.IsNearlyZero()) return DesiredDir;

	const float CurrentYaw = FMath::Atan2(CurrentFaceDir.Y, CurrentFaceDir.X) * (180.f / UE_PI);
	const float DesiredYaw = FMath::Atan2(DesiredDir.Y, DesiredDir.X) * (180.f / UE_PI);
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw);
	const float MaxStep = WarpRotationSpeedDegPerSec * DeltaTime;
	const float ClampedDelta = FMath::Clamp(DeltaYaw, -MaxStep, MaxStep);
	const float FinalYaw = CurrentYaw + ClampedDelta;
	const float Rad = FMath::DegreesToRadians(FinalYaw);
	return FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);
}

static constexpr float WarpPositionUpdateThresholdSq = 1.f;   // 1cm squared
static constexpr float WarpRotationUpdateThresholdDeg = 0.1f;  // 0.1 degrees

void UGA_OSAttack::RefreshWarpTarget(ACharacter* Character, float DeltaTime)
{
	if (!IsValid(Character)) return;

	UMotionWarpingComponent* MWC = CachedMWC.Get();
	if (!MWC) return;

	const FName ActiveName = GetWarpTargetName();
	const FMotionWarpingTarget* Current = MWC->FindWarpTarget(ActiveName);
	if (!Current) return;

	const FVector OwnerLoc = Character->GetActorLocation();

	// Use the mode locked at injection time — don't re-evaluate Target each frame.
	const bool bHasTarget = bWarpHasTarget && IsValid(Target.Get()) && !UOSCombatBlueprintLibrary::IsIncapacitated(Target.Get());

	// --- Position ---
	// No-target path: position was set at injection and can't change (no actor to track).
	// Only the has-target path benefits from position refresh.
	FVector WarpLoc = Current->GetLocation();
	if (bRefreshWarpPosition && bHasTarget)
	{
		ComputeWarpPositionWithTarget(Character, OwnerLoc, WarpLoc);
	}

	// --- Rotation ---
	FRotator WarpRot = Current->GetRotation().Rotator();
	if (bRefreshWarpRotation)
	{
		const FVector InputDir = UOSCombatBlueprintLibrary::GetInputDirection2D(Character);
		FVector FaceDir;
		if (ComputeWarpRotation(Character, InputDir, FaceDir))
		{
			FaceDir.Z = 0.f;
			FaceDir = FaceDir.GetSafeNormal();
			if (!FaceDir.IsNearlyZero())
			{
				FaceDir = ApplyRotationDamping(MWC, FaceDir, DeltaTime);
				WarpRot = FRotator(0.f, FaceDir.Rotation().Yaw, 0.f);
			}
		}
	}

	// STABILIZATION: Only update the component if the change is meaningful.
	const bool bNeedsPositionUpdate = bRefreshWarpPosition && bHasTarget
		&& FVector::DistSquared(Current->GetLocation(), WarpLoc) >= WarpPositionUpdateThresholdSq;
	const bool bNeedsRotationUpdate = bRefreshWarpRotation
		&& FMath::Abs(FMath::FindDeltaAngleDegrees(Current->GetRotation().Rotator().Yaw, WarpRot.Yaw)) >= WarpRotationUpdateThresholdDeg;

	if (bNeedsPositionUpdate || bNeedsRotationUpdate)
	{
		const FTransform UpdatedTransform(WarpRot, WarpLoc);
		MWC->AddOrUpdateWarpTargetFromTransform(ActiveName, UpdatedTransform);

		// Push updated warp target to proxies for root motion matching
		if (HasAuthority(&CurrentActivationInfo))
		{
			if (AOSCharacter* OSChar = Cast<AOSCharacter>(Character))
			{
				OSChar->SetReplicatedWarpTarget(ActiveName, UpdatedTransform);
			}
		}
	}

#if !UE_BUILD_SHIPPING
	// GHOST TRAIL: visualize rotation history
	DebugGhostTimer += DeltaTime;
	if (DebugGhostTimer >= 0.05f) // 20hz ghosts
	{
		DebugGhostTimer = 0.f;
		DebugOrientationGhosts.Add(Character->GetActorTransform());
		if (DebugOrientationGhosts.Num() > 30) DebugOrientationGhosts.RemoveAt(0);
	}
	DebugLastTrackedLoc = OwnerLoc;
#endif
}

void UGA_OSAttack::ClearWarpTarget(ACharacter* Character) const
{
	if (!IsValid(Character)) return;

	if (UMotionWarpingComponent* MWC = Character->FindComponentByClass<UMotionWarpingComponent>())
	{
#if !UE_BUILD_SHIPPING
		// Diagnostic: log modifier states at EndAbility cleanup time.
		const TArray<URootMotionModifier*>& Mods = MWC->GetModifiers();
		for (const URootMotionModifier* Mod : Mods)
		{
			const TCHAR* StateStr = TEXT("Unknown");
			switch (Mod->GetState())
			{
			case ERootMotionModifierState::Waiting:          StateStr = TEXT("Waiting"); break;
			case ERootMotionModifierState::Active:            StateStr = TEXT("Active"); break;
			case ERootMotionModifierState::MarkedForRemoval:  StateStr = TEXT("MarkedForRemoval"); break;
			case ERootMotionModifierState::Disabled:          StateStr = TEXT("Disabled"); break;
			}

			const URootMotionModifier_Warp* WarpMod = Cast<URootMotionModifier_Warp>(Mod);
			UE_LOG(LogOSAttack, Log,
				TEXT("[%s] EndAbility cleanup: modifier state=%s anim=%s [%.3f-%.3f] warpName=%s | owner=%s"),
				Character->HasAuthority() ? TEXT("SRV") : TEXT("CLT"),
				StateStr,
				*GetNameSafe(Mod->GetAnimation()),
				Mod->StartTime, Mod->EndTime,
				WarpMod ? *WarpMod->WarpTargetName.ToString() : TEXT("N/A"),
				*Character->GetName());
		}
#endif

		// Remove the warp target. The engine auto-disables modifiers whose target name
		// no longer exists (URootMotionModifier_Warp::Update). No DisableAll — killing
		// modifiers mid-blend-out causes raw root motion to play, producing slingshot
		// on the last combo hit.
		MWC->RemoveWarpTarget(GetWarpTargetName());
	}

	// Clear replicated warp target so proxies also remove it
	if (AOSCharacter* OSChar = Cast<AOSCharacter>(Character))
	{
		OSChar->ClearReplicatedWarpTarget();
	}
}

bool UGA_OSAttack::HasMeleeOptimalDistance() const
{
	return true;
}

float UGA_OSAttack::GetMeleeOptimalDistance() const
{
	return OptimalMeleeDistance;
}

// ========================================
// ATTACK PHASE TRACKING
// ========================================

/** Resolve phase enum → gameplay tag. */
static FGameplayTag GetPhaseTag(EOSAttackPhase Phase)
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	switch (Phase)
	{
	case EOSAttackPhase::Windup:   return Tags.State_Attack_Windup;
	case EOSAttackPhase::Active:   return Tags.State_Attack_Active;
	case EOSAttackPhase::Recovery: return Tags.State_Attack_Recovery;
	default:                       return FGameplayTag();
	}
}

void UGA_OSAttack::SetPhase(EOSAttackPhase NewPhase)
{
	if (CurrentPhase == NewPhase) return;

	UE_LOG(LogOSAttack, Log, TEXT("[PHASE] SetPhase: %d → %d"), (int32)CurrentPhase, (int32)NewPhase);

	UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo();
	if (!AbilityASC) return;

	const EOSAttackPhase OldPhase = CurrentPhase;

	// Remove outgoing phase tag (set count to 0 — clean removal, no counting bugs)
	const FGameplayTag OldTag = GetPhaseTag(OldPhase);
	if (OldTag.IsValid())
	{
		AbilityASC->SetReplicatedLooseGameplayTagCount(OldTag, 0);
	}

	CurrentPhase = NewPhase;

	// Grant incoming phase tag (replicated in Mixed mode via SetReplicatedLooseGameplayTagCount)
	const FGameplayTag NewTag = GetPhaseTag(NewPhase);
	if (NewTag.IsValid())
	{
		AbilityASC->SetReplicatedLooseGameplayTagCount(NewTag, 1);
	}

	OnPhaseChanged(OldPhase, NewPhase);
}

void UGA_OSAttack::ClearPhaseState()
{
	UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo();

	// Remove active phase tag
	const FGameplayTag ActiveTag = GetPhaseTag(CurrentPhase);
	if (ActiveTag.IsValid() && AbilityASC)
	{
		AbilityASC->SetReplicatedLooseGameplayTagCount(ActiveTag, 0);
	}

	CurrentPhase = EOSAttackPhase::None;

	// Clean up phase event tasks
	if (UAbilityTask_WaitGameplayEvent* Task = PhaseActiveTask.Get())
	{
		if (IsValid(Task) && Task->IsActive()) Task->EndTask();
		PhaseActiveTask = nullptr;
	}
	if (UAbilityTask_WaitGameplayEvent* Task = PhaseRecoveryTask.Get())
	{
		if (IsValid(Task) && Task->IsActive()) Task->EndTask();
		PhaseRecoveryTask = nullptr;
	}
}

void UGA_OSAttack::StartPhaseTracking()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	// Set initial Windup phase
	SetPhase(EOSAttackPhase::Windup);

	// Create persistent listeners for phase events (survive across combo section jumps).
	// OnlyTriggerOnce=false: tasks fire every time the ANS_OSAttackPhase notify sends the event.
	if (!IsValid(PhaseActiveTask.Get()))
	{
		PhaseActiveTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, Tags.Event_Attack_Phase_Active, nullptr, false, false);
		if (IsValid(PhaseActiveTask.Get()))
		{
			PhaseActiveTask->EventReceived.AddDynamic(this, &UGA_OSAttack::OnPhaseActiveEvent);
			PhaseActiveTask->ReadyForActivation();
		}
	}

	if (!IsValid(PhaseRecoveryTask.Get()))
	{
		PhaseRecoveryTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, Tags.Event_Attack_Phase_Recovery, nullptr, false, false);
		if (IsValid(PhaseRecoveryTask.Get()))
		{
			PhaseRecoveryTask->EventReceived.AddDynamic(this, &UGA_OSAttack::OnPhaseRecoveryEvent);
			PhaseRecoveryTask->ReadyForActivation();
		}
	}
}

void UGA_OSAttack::OnPhaseActiveEvent(FGameplayEventData Payload)
{
	SetPhase(EOSAttackPhase::Active);
}

void UGA_OSAttack::OnPhaseRecoveryEvent(FGameplayEventData Payload)
{
	SetPhase(EOSAttackPhase::Recovery);
}

