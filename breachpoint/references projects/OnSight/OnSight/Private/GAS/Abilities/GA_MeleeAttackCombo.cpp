#include "GAS/Abilities/GA_MeleeAttackCombo.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/OSCharacter.h"
#include "Data/OSGameplayTags.h"
#include "MotionWarpingComponent.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

class AActor;

/** Same HUD hook as UGA_OSAttack/UGA_OSSoftLock: GenericGameplayEventCallbacks listen for UI_TargetChanged. */
static void BroadcastTargetChangedForUI(UGameplayAbility* Ability, AActor* Target)
{
	if (!Ability)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = Ability->GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayEventData Payload;
		Payload.Target = Target;
		ASC->HandleGameplayEvent(FOSGameplayTags::Get().UI_TargetChanged, &Payload);
	}
}

UGA_MeleeAttackCombo::UGA_MeleeAttackCombo()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	TrackingStateTag = FGameplayTag::RequestGameplayTag(TEXT("Character.State.Tracking"));

	// NOTE:
	// This class is intended as a reusable base for both Light and Heavy combo abilities.
	// Do NOT hardcode asset tags here. Set them on the BP child CDO (or in a derived C++ class) via SetAssetTags.

	// UOSGameplayAbility: IsDead. Align CC/grab lockout with UGA_OSAttack; combo-specific: block/dodge/sprint/air.
	ActivationBlockedTags.AddTag(Tags.IsStunned);
	ActivationBlockedTags.AddTag(Tags.IsHitReacting);
	ActivationBlockedTags.AddTag(Tags.IsRecoiling);
	ActivationBlockedTags.AddTag(Tags.IsKnockedDown);
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsBeingGrabbed);
	ActivationBlockedTags.AddTag(Tags.State_IsStunLock);
	ActivationBlockedTags.AddTag(Tags.IsMantling);
	ActivationBlockedTags.AddTag(Tags.IsBlocking);
	ActivationBlockedTags.AddTag(Tags.IsSprinting);
	ActivationBlockedTags.AddTag(Tags.IsInAir);

	ActivationOwnedTags.AddTag(Tags.IsAttacking);

	// Allow attacking during dodge: when this ability activates it will cancel the dodge ability.
	if (Tags.Ability_Dodge.IsValid())
	{
		CancelAbilitiesWithTag.AddTag(Tags.Ability_Dodge);
	}

	SoftTargetExcludeTags.AddTag(Tags.IsDead);
	SoftTargetExcludeTags.AddTag(Tags.IsStunned);
	SoftTargetExcludeTags.AddTag(Tags.IsGrabbed);
}

void UGA_MeleeAttackCombo::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// UOSGameplayAbility commits cost/cooldown here and ends on failure.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	AOSCharacter* OwnerChar = Avatar();
	if (!IsValid(OwnerChar) || !IsValid(ComboMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Safety net for stamina regen inhibition:
	// GE_OSStaminaRegen ignores IsAttacking; ensure the tag exists even if a BP CDO accidentally
	// removed it from ActivationOwnedTags for this ability.
	bAddedIsAttackingLooseTag = false;
	if (UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		if (Tags.IsAttacking.IsValid() && !AbilityASC->HasMatchingGameplayTag(Tags.IsAttacking))
		{
			AbilityASC->AddReplicatedLooseGameplayTag(Tags.IsAttacking);
			bAddedIsAttackingLooseTag = true;
		}
	}

	CurrentHitIndex = 0;
	bPendingComboInput = false;
	bComboBufferedTagSet = false;
	Phase = EMeleeComboPhase::None;
	BufferedCrossTypeTag = FGameplayTag();
	CurrentTarget = nullptr;
	PersistedTarget = nullptr;
	StoredBlendedDirection2D = FVector::ZeroVector;

	StartPersistentEventListeners();
	StartHit(0);
}

void UGA_MeleeAttackCombo::StartPersistentEventListeners()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	// Combo input
	if (Tags.Event_ComboInputPressed.IsValid())
	{
		WaitComboInputTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, Tags.Event_ComboInputPressed, nullptr, false, false);

		if (IsValid(WaitComboInputTask))
		{
			WaitComboInputTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackCombo::OnComboInputReceived);
			WaitComboInputTask->ReadyForActivation();
		}
	}

	// Phase events (from ANS attack phase notify windows)
	if (Tags.Event_Attack_Phase_Active.IsValid())
	{
		WaitPhaseActiveTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, Tags.Event_Attack_Phase_Active, nullptr, false, false);

		if (IsValid(WaitPhaseActiveTask))
		{
			WaitPhaseActiveTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackCombo::OnPhaseActiveReceived);
			WaitPhaseActiveTask->ReadyForActivation();
		}
	}

	if (Tags.Event_Attack_Phase_Recovery.IsValid())
	{
		WaitPhaseRecoveryTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, Tags.Event_Attack_Phase_Recovery, nullptr, false, false);

		if (IsValid(WaitPhaseRecoveryTask))
		{
			WaitPhaseRecoveryTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackCombo::OnPhaseRecoveryReceived);
			WaitPhaseRecoveryTask->ReadyForActivation();
		}
	}

	// Frame-accurate damage trigger (server fires this notify)
	if (Tags.Event_DirectDamage.IsValid())
	{
		WaitDirectDamageTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, Tags.Event_DirectDamage, nullptr, false, false);

		if (IsValid(WaitDirectDamageTask))
		{
			WaitDirectDamageTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackCombo::OnDirectDamageReceived);
			WaitDirectDamageTask->ReadyForActivation();
		}
	}
}

FName UGA_MeleeAttackCombo::GetSectionNameForHit(int32 HitIndex) const
{
	if (ComboSectionNames.IsValidIndex(HitIndex))
	{
		return ComboSectionNames[HitIndex];
	}

	if (IsValid(ComboMontage))
	{
		if (HitIndex >= 0 && HitIndex < ComboMontage->GetNumSections())
		{
			return ComboMontage->GetSectionName(HitIndex);
		}
	}

	return NAME_None;
}

void UGA_MeleeAttackCombo::StartHit(int32 HitIndex)
{
	if (HitIndex > 0 && bSpendCostsPerHit)
	{
		if (!CheckCost(CurrentSpecHandle, CurrentActorInfo))
		{
			OSEndAbility();
			return;
		}

		ApplyCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	}

	AOSCharacter* OwnerChar = Avatar();
	if (!IsValid(OwnerChar) || !IsValid(ComboMontage))
	{
		OSEndAbility();
		return;
	}

	const FName SectionName = GetSectionNameForHit(HitIndex);
	if (SectionName.IsNone())
	{
		OSEndAbility();
		return;
	}

	CurrentHitIndex = HitIndex;
	Phase = EMeleeComboPhase::None;
	BufferedCrossTypeTag = FGameplayTag();

	ResolveSoftTarget_WithSoftLock(OwnerChar);
	InjectWarpTarget(OwnerChar);

	if (IsValid(ActiveMontageTask) && ActiveMontageTask->IsActive())
	{
		ActiveMontageTask->EndTask();
		ActiveMontageTask = nullptr;
	}

	ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(*FString::Printf(TEXT("MeleeComboHit_%d"), HitIndex)),
		ComboMontage,
		1.0f,
		SectionName,
		false,
		1.0f
	);

	if (!IsValid(ActiveMontageTask))
	{
		OSEndAbility();
		return;
	}

	ActiveMontageTask->OnBlendOut.AddDynamic(this, &UGA_MeleeAttackCombo::OnHitMontageEnded);
	ActiveMontageTask->OnCompleted.AddDynamic(this, &UGA_MeleeAttackCombo::OnHitMontageEnded);
	ActiveMontageTask->OnInterrupted.AddDynamic(this, &UGA_MeleeAttackCombo::OnHitMontageEnded);
	ActiveMontageTask->OnCancelled.AddDynamic(this, &UGA_MeleeAttackCombo::OnHitMontageEnded);
	ActiveMontageTask->ReadyForActivation();
}

float UGA_MeleeAttackCombo::GetPersistMaxRange() const
{
	return PersistMaxRangeOverride > 0.f ? PersistMaxRangeOverride : SoftTargetMaxRange;
}

bool UGA_MeleeAttackCombo::IsValidTargetForPersist(AOSCharacter* OwnerChar, AOSCharacter* Target, float MaxRange) const
{
	if (!IsValid(OwnerChar) || !IsValid(Target) || Target == OwnerChar)
	{
		return false;
	}

	if (UOSCombatBlueprintLibrary::IsIncapacitated(Target))
	{
		return false;
	}

	const float DistSq = FVector::DistSquared(OwnerChar->GetActorLocation(), Target->GetActorLocation());
	return DistSq <= FMath::Square(FMath::Max(0.f, MaxRange));
}

void UGA_MeleeAttackCombo::SetLockedOnState(bool bLockedOn)
{
	if (UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FGameplayTag& LockedOnTag = FOSGameplayTags::Get().IsLockedOn;
		if (LockedOnTag.IsValid())
		{
			AbilityASC->SetReplicatedLooseGameplayTagCount(LockedOnTag, bLockedOn ? 1 : 0);
		}
	}
}

void UGA_MeleeAttackCombo::ResolveSoftTarget_WithSoftLock(AOSCharacter* OwnerChar)
{
	AActor* OldTarget = CurrentTarget.Get();

	CurrentTarget = nullptr;
	StoredBlendedDirection2D = FVector::ZeroVector;

	if (!IsValid(OwnerChar) || UOSCombatBlueprintLibrary::IsIncapacitated(OwnerChar))
	{
		SetLockedOnState(false);
		if (bBroadcastTargetChangedToUI && OldTarget)
		{
			BroadcastTargetChangedForUI(this, nullptr);
		}
		return;
	}

	UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo();
	const bool bTrackingActive = AbilityASC && TrackingStateTag.IsValid()
		? AbilityASC->HasMatchingGameplayTag(TrackingStateTag)
		: false;

	const float PersistRange = GetPersistMaxRange();

	// 1) If tracking is active, prefer the replicated soft-lock target on the avatar.
	if (bPreferReplicatedSoftLockTarget && bTrackingActive)
	{
		AOSCharacter* SoftLockChar = Cast<AOSCharacter>(OwnerChar->SoftLockTarget);
		if (IsValidTargetForPersist(OwnerChar, SoftLockChar, PersistRange))
		{
			CurrentTarget = SoftLockChar;
			const FVector ToTarget2D = (SoftLockChar->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal2D();
			StoredBlendedDirection2D = ToTarget2D.IsNearlyZero()
				? OwnerChar->GetActorForwardVector().GetSafeNormal2D()
				: ToTarget2D;
		}
	}

	// 2) Combo persistence (optional): keep using the last resolved target if still valid/in range.
	if (!CurrentTarget.IsValid() && bPersistTargetAcrossChain)
	{
		AOSCharacter* PersistTarget = PersistedTarget.Get();
		if (IsValidTargetForPersist(OwnerChar, PersistTarget, PersistRange))
		{
			CurrentTarget = PersistTarget;
			const FVector ToTarget2D = (PersistTarget->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal2D();
			StoredBlendedDirection2D = ToTarget2D.IsNearlyZero()
				? OwnerChar->GetActorForwardVector().GetSafeNormal2D()
				: ToTarget2D;
		}
	}

	// 3) Soft targeting cone (optional): resolve a fresh target + blended direction.
	if (!CurrentTarget.IsValid() && bSoftTargeting)
	{
		FOSAttackDirectionParams Params;
		Params.ConeAngleDegrees = SoftTargetConeAngle;
		Params.MaxRange = SoftTargetMaxRange;
		Params.ExcludeTags = SoftTargetExcludeTags;

		Params.BlendAlgorithm = BlendAlgorithm;
		Params.InputInfluence = InputInfluence;
		Params.MagneticStickiness = MagneticStickiness;
		Params.MagneticBreakawayAngle = MagneticBreakawayAngle;

		FVector FinalDir2D = FVector::ZeroVector;
		AOSCharacter* ResolvedTarget = nullptr;
		bool bFound = UOSCombatBlueprintLibrary::ResolveAttackDirectionInCone(
			OwnerChar, Params, FinalDir2D, ResolvedTarget);

		// AI fallback: retry with full-sphere search.
		if (!bFound && !OwnerChar->IsPlayerControlled())
		{
			FOSAttackDirectionParams WideParams = Params;
			WideParams.ConeAngleDegrees = 360.f;
			bFound = UOSCombatBlueprintLibrary::ResolveAttackDirectionInCone(
				OwnerChar, WideParams, FinalDir2D, ResolvedTarget);
		}

		CurrentTarget = ResolvedTarget;
		StoredBlendedDirection2D = bFound ? FinalDir2D : FVector::ZeroVector;
	}

	// 4) Fallback direction
	if (StoredBlendedDirection2D.IsNearlyZero())
	{
		StoredBlendedDirection2D = UOSCombatBlueprintLibrary::GetAimDirection2D(OwnerChar);
	}

	if (StoredBlendedDirection2D.IsNearlyZero())
	{
		StoredBlendedDirection2D = OwnerChar->GetActorForwardVector().GetSafeNormal2D();
	}

	if (CurrentTarget.IsValid())
	{
		if (bPersistTargetAcrossChain)
		{
			PersistedTarget = CurrentTarget;
		}
		SetLockedOnState(true);
	}
	else
	{
		SetLockedOnState(false);
	}

	if (bBroadcastTargetChangedToUI && CurrentTarget.Get() != OldTarget)
	{
		BroadcastTargetChangedForUI(this, CurrentTarget.Get());
	}
}

void UGA_MeleeAttackCombo::InjectWarpTarget(AOSCharacter* OwnerChar)
{
	if (!IsValid(OwnerChar) || WarpTargetName.IsNone())
	{
		return;
	}

	UMotionWarpingComponent* MWC = OwnerChar->GetMotionWarpingComponent();
	if (!MWC)
	{
		return;
	}

	const FVector OwnerLoc = OwnerChar->GetActorLocation();
	const FVector FaceDir2D = StoredBlendedDirection2D.GetSafeNormal2D().IsNearlyZero()
		? OwnerChar->GetActorForwardVector().GetSafeNormal2D()
		: StoredBlendedDirection2D.GetSafeNormal2D();

	FVector WarpLoc = OwnerLoc;
	FRotator WarpRot(0.f, FaceDir2D.Rotation().Yaw, 0.f);

	AOSCharacter* Target = CurrentTarget.Get();
	if (IsValid(Target) && !UOSCombatBlueprintLibrary::IsIncapacitated(Target))
	{
		const FVector TargetLoc = Target->GetActorLocation();

		const float CapsuleStop = UOSCombatBlueprintLibrary::GetCapsuleStopDistance(OwnerChar, Target);
		const float Standoff = FMath::Max(OptimalMeleeDistance, CapsuleStop);

		const FVector ToTarget2D = (TargetLoc - OwnerLoc).GetSafeNormal2D();
		const FVector Dir2D = ToTarget2D.IsNearlyZero() ? FaceDir2D : ToTarget2D;

		WarpRot = FRotator(0.f, Dir2D.Rotation().Yaw, 0.f);
		WarpLoc = TargetLoc - Dir2D * Standoff;
		WarpLoc.Z = OwnerLoc.Z;
	}
	else if (DirectionalFallbackWarpDistance > 0.f)
	{
		WarpLoc = OwnerLoc + FaceDir2D * DirectionalFallbackWarpDistance;
		WarpLoc.Z = OwnerLoc.Z;
	}

	MWC->AddOrUpdateWarpTargetFromTransform(WarpTargetName, FTransform(WarpRot, WarpLoc));
}

void UGA_MeleeAttackCombo::ClearWarpTarget(AOSCharacter* OwnerChar)
{
	if (!IsValid(OwnerChar) || WarpTargetName.IsNone())
	{
		return;
	}

	if (UMotionWarpingComponent* MWC = OwnerChar->GetMotionWarpingComponent())
	{
		MWC->RemoveWarpTarget(WarpTargetName);
	}
}

FGameplayTag UGA_MeleeAttackCombo::GetEffectiveInputTag() const
{
	if (InputActivationTag.IsValid())
	{
		return InputActivationTag;
	}

	// Safety net: infer type from this ability's own asset tags so cross-type switching works
	// even if designers forgot to set InputActivationTag on the BP child.
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	const FGameplayTagContainer& AssetTags = GetAssetTags();

	if (Tags.Attack_Heavy.IsValid() && AssetTags.HasTagExact(Tags.Attack_Heavy))
	{
		return Tags.Attack_Heavy;
	}
	if (Tags.Attack_Light.IsValid() && AssetTags.HasTagExact(Tags.Attack_Light))
	{
		return Tags.Attack_Light;
	}

	return FGameplayTag();
}

void UGA_MeleeAttackCombo::SwitchToAbilityByTag(const FGameplayTag& SwitchToTag)
{
	if (!SwitchToTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo();

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);

	if (IsValid(AbilityASC))
	{
		FGameplayTagContainer ActivateTags;
		ActivateTags.AddTag(SwitchToTag);
		AbilityASC->TryActivateAbilitiesByTag(ActivateTags);
	}
}

void UGA_MeleeAttackCombo::OnComboInputReceived(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	const FGameplayTag RequestedTypeTag = (Payload.InstigatorTags.Num() > 0) ? Payload.InstigatorTags.First() : FGameplayTag();
	const FGameplayTag EffectiveInputTag = GetEffectiveInputTag();

	const bool bCrossType = EffectiveInputTag.IsValid() && RequestedTypeTag.IsValid()
		&& !RequestedTypeTag.MatchesTagExact(EffectiveInputTag);

	if (Phase == EMeleeComboPhase::Recovery)
	{
		// Recovery + cross-type: end now and activate the new type.
		// Controller input already happened while IsAttacking was still present, so it will NOT auto-activate.
		if (bCrossType)
		{
			SwitchToAbilityByTag(RequestedTypeTag);
			return;
		}

		if (bRecoveryPressRestartsCombo)
		{
			CurrentHitIndex = 0;
			bPendingComboInput = false;
			BufferedCrossTypeTag = FGameplayTag();
			StartHit(0);
		}
		else
		{
			BufferedCrossTypeTag = FGameplayTag();
			StartHit(CurrentHitIndex + 1);
		}
		return;
	}

	bPendingComboInput = true;
	BufferedCrossTypeTag = bCrossType ? RequestedTypeTag : FGameplayTag();

	if (!bComboBufferedTagSet)
	{
		if (UAbilitySystemComponent* A = GetAbilitySystemComponentFromActorInfo())
		{
			A->SetReplicatedLooseGameplayTagCount(FOSGameplayTags::Get().State_Attack_ComboBuffered, 1);
		}
		bComboBufferedTagSet = true;
	}
}

void UGA_MeleeAttackCombo::OnPhaseActiveReceived(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	Phase = EMeleeComboPhase::Active;
}

void UGA_MeleeAttackCombo::OnPhaseRecoveryReceived(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	Phase = EMeleeComboPhase::Recovery;

	if (bPendingComboInput)
	{
		// Consume the pending input.
		bPendingComboInput = false;

		// Cross-type buffered during Windup/Active: end self and activate the requested type.
		if (BufferedCrossTypeTag.IsValid())
		{
			const FGameplayTag SwitchToTag = BufferedCrossTypeTag;
			BufferedCrossTypeTag = FGameplayTag();
			SwitchToAbilityByTag(SwitchToTag);
			return;
		}

		StartHit(CurrentHitIndex + 1);
	}
}

void UGA_MeleeAttackCombo::OnDirectDamageReceived(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	// Gameplay is server-authoritative. AN_OSDirectDamage only fires on server.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	AOSCharacter* OwnerChar = Avatar();
	if (!IsValid(OwnerChar))
	{
		return;
	}

	// AN_OSDirectDamage sends Payload.Target = Owner (timing signal only), so by default we use our resolved target.
	// If some future notify/event chooses to put an actual victim into Payload.Target, it will come through as const.
	AOSCharacter* Victim = CurrentTarget.Get();
	if (!IsValid(Victim))
	{
		if (const AOSCharacter* PayloadVictimConst = Cast<AOSCharacter>(Payload.Target))
		{
			if (PayloadVictimConst != OwnerChar)
			{
				Victim = const_cast<AOSCharacter*>(PayloadVictimConst);
			}
		}
	}

	if (!IsValid(Victim))
	{
		return;
	}

	UOSCombatBlueprintLibrary::ApplyDirectDamage(OwnerChar, Victim, DamageEffectClass, BaseDamage, AttackType);
}

void UGA_MeleeAttackCombo::OnHitMontageEnded()
{
	if (!IsActive())
	{
		return;
	}

	// Fallback: if we never got a Recovery notify but input was buffered, attempt to advance.
	if (bPendingComboInput)
	{
		bPendingComboInput = false;

		// Same behavior as Recovery: cross-type buffered should switch, otherwise advance.
		if (BufferedCrossTypeTag.IsValid())
		{
			const FGameplayTag SwitchToTag = BufferedCrossTypeTag;
			BufferedCrossTypeTag = FGameplayTag();
			SwitchToAbilityByTag(SwitchToTag);
			return;
		}

		StartHit(CurrentHitIndex + 1);
		return;
	}

	OSEndAbility();
}

void UGA_MeleeAttackCombo::CleanupTasks()
{
	auto EndIfActive = [](UAbilityTask* Task)
	{
		if (IsValid(Task) && Task->IsActive())
		{
			Task->EndTask();
		}
	};

	EndIfActive(ActiveMontageTask);
	EndIfActive(WaitComboInputTask);
	EndIfActive(WaitPhaseActiveTask);
	EndIfActive(WaitPhaseRecoveryTask);
	EndIfActive(WaitDirectDamageTask);

	ActiveMontageTask = nullptr;
	WaitComboInputTask = nullptr;
	WaitPhaseActiveTask = nullptr;
	WaitPhaseRecoveryTask = nullptr;
	WaitDirectDamageTask = nullptr;
}

void UGA_MeleeAttackCombo::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	AActor* OldTarget = CurrentTarget.Get();

	if (AOSCharacter* OwnerChar = Avatar())
	{
		ClearWarpTarget(OwnerChar);
	}

	if (bAddedIsAttackingLooseTag)
	{
		if (UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo())
		{
			const FGameplayTag Tag = FOSGameplayTags::Get().IsAttacking;
			if (Tag.IsValid())
			{
				AbilityASC->RemoveReplicatedLooseGameplayTag(Tag);
			}
		}
		bAddedIsAttackingLooseTag = false;
	}

	if (bComboBufferedTagSet)
	{
		if (UAbilitySystemComponent* A = GetAbilitySystemComponentFromActorInfo())
		{
			A->SetReplicatedLooseGameplayTagCount(FOSGameplayTags::Get().State_Attack_ComboBuffered, 0);
		}
		bComboBufferedTagSet = false;
	}

	// Reset combo state aggressively on any end/cancel so the next activation always starts at hit 0.
	CurrentHitIndex = 0;
	bPendingComboInput = false;
	Phase = EMeleeComboPhase::None;
	CurrentTarget = nullptr;
	PersistedTarget = nullptr;
	BufferedCrossTypeTag = FGameplayTag();
	StoredBlendedDirection2D = FVector::ZeroVector;

	SetLockedOnState(false);
	if (bBroadcastTargetChangedToUI && OldTarget)
	{
		BroadcastTargetChangedForUI(this, nullptr);
	}

	CleanupTasks();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MeleeAttackCombo::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (AOSCharacter* OwnerChar = GetOwningCharacter())
	{
		ClearWarpTarget(OwnerChar);
	}

	CleanupTasks();

	Super::OnRemoveAbility(ActorInfo, Spec);
}

