#include "GAS/Abilities/GA_OSComboAttack.h"
#include "AbilitySystemComponent.h"
#include "Characters/OSCharacter.h"
#include "Data/OSGameplayTags.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"
#include "OSLogCategories.h"

UGA_OSComboAttack::UGA_OSComboAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// Inherits ReplicateYes from GA_OSAttack — required for ClientRPC_SyncWarpState delivery.
	// Warp state (direction, has-target) is synced via reliable Client RPC after server injection.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	// AssetTags (activation + cancel queries) are set in the BP CDO.
	// Light BP has Attack + Attack_Light, Heavy BP has Attack + Attack_Heavy.

	// NOTE: Blueprint CDO serialization may override these values if the BP subclass was saved
	// without them. CanActivateAbility below is the C++ authoritative enforcement.
	ActivationBlockedTags.AddTag(Tags.IsDead);
	ActivationBlockedTags.AddTag(Tags.IsSprinting);
	ActivationBlockedTags.AddTag(Tags.IsInAir);
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);

	// IsAttacking is a state tag granted while the attack is active. Using ActivationOwnedTags
	// (not loose tags) so it replicates correctly in Mixed mode. Matches GA_OSBlock's IsBlocking pattern.
	// NOTE: BP CDO serialization overrides ActivationOwnedTags entirely for existing subclasses.
	// Existing BPs (GA_ComboAttack, GA_ComboAttackHeavy) MUST include IsAttacking in their
	// ActivationOwnedTags in the editor. This line only provides the default for new subclasses.
	ActivationOwnedTags.AddTag(Tags.IsAttacking);

	// bSoftTargeting defaults to false on the base class; BP CDO overrides to true for melee combos.
	// Do NOT hardcode bSoftTargeting here — it prevents designers from toggling it per-ability.
	SoftTargetExcludeTags.AddTag(Tags.IsDead);
	SoftTargetExcludeTags.AddTag(Tags.IsStunned);
	SoftTargetExcludeTags.AddTag(Tags.IsGrabbed);
	BlendAlgorithm = EOSBlendAlgorithm::Magnetic;

	// Default to light attack type. Heavy BP subclass overrides to Attack_Heavy.
	// NOTE FOR ALEX: This drives TryCombo's type-switch logic. If the BP doesn't override it,
	// all combo hits within this ability instance use the same type (index never resets mid-chain).
	AttackTypeTag = Tags.Attack_Light;
}

// ========================================
// COMBO BUFFER TAG (single source of truth)
// ========================================

void UGA_OSComboAttack::SetComboBuffered(bool bBuffered)
{
	bPendingComboInput = bBuffered;
	// Tag only goes UP here (set on first buffer). Cleared exclusively in CleanupComboState.
	// This keeps the tag alive for the entire combo chain so Move() never cancels mid-combo.
	if (bBuffered)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->SetReplicatedLooseGameplayTagCount(FOSGameplayTags::Get().State_Attack_ComboBuffered, 1);
		}
	}
}

// ========================================
// COMBO FLOW
// ========================================

void UGA_OSComboAttack::TryAttack( const FGameplayTag& AttackType )
{
	UE_LOG(LogOSAttack, Log, TEXT("[COMBO] TryAttack: Phase=%d IsPhaseValid=%d ComboIndex=%d"),
		(int32)CurrentPhase, IsPhaseValid(), CurrentComboIndex);
	if (!IsPhaseValid())
	{
		UE_LOG(LogOSAttack, Log, TEXT("[COMBO] TryAttack: BUFFERED same-type (Phase=%d blocks execution)"),
			(int32)CurrentPhase);
		SetComboBuffered(true);
		BufferedCrossTypeTag = FGameplayTag();
		return;
	}
	TryCombo(AttackType);
}

void UGA_OSComboAttack::TryCombo( const FGameplayTag& AttackType )
{
	bool bSameType = PreviousAttackType.IsValid() && PreviousAttackType.MatchesTagExact(AttackType);
	GetEffectiveSectionNames(EffectiveSectionNames);
	bool bValidIndex = CurrentComboIndex < EffectiveSectionNames.Num();

	UE_LOG(LogOSAttack, Log, TEXT("[COMBO] TryCombo: sameType=%d validIndex=%d index=%d sections=%d"),
		bSameType, bValidIndex, CurrentComboIndex, EffectiveSectionNames.Num());
	
	if (!ApplyCostPerHit())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}
	
	if (!bSameType || !bValidIndex)
		CurrentComboIndex = 0;

	PreviousAttackType = AttackType;
	PerformComboHit(CurrentComboIndex);
	CurrentComboIndex++;
}

bool UGA_OSComboAttack::IsPhaseValid() const
{
	// None: first hit (ability just activated, no montage yet)
	// Recovery: combo window (hitbox closed, punishable — this replaces the old Combo_CanQueue tag)
	// Windup/Active: committed to current attack — input must buffer, not interrupt
	return CurrentPhase == EOSAttackPhase::None || CurrentPhase == EOSAttackPhase::Recovery;
}

void UGA_OSComboAttack::OnPhaseChanged( EOSAttackPhase OldPhase, EOSAttackPhase NewPhase )
{
	UE_LOG(LogOSAttack, Log, TEXT("[COMBO] OnPhaseChanged: %d → %d, pendingInput=%d, crossType=%d"),
		(int32)OldPhase, (int32)NewPhase, bPendingComboInput, BufferedCrossTypeTag.IsValid());
	Super::OnPhaseChanged(OldPhase, NewPhase);
	if (NewPhase == EOSAttackPhase::Recovery && bPendingComboInput)
	{
		const FGameplayTag CrossTag = BufferedCrossTypeTag;
		// Consume the pending input but DO NOT clear the tag — tag stays set for Move() protection.
		bPendingComboInput = false;
		BufferedCrossTypeTag = FGameplayTag();

		if (CrossTag.IsValid())
		{
			// Cross-type buffer: end self, then activate the new type.
			// EndAbility removes IsAttacking via ActivationOwnedTags (synchronous in GAS).
			// We must explicitly TryActivate because the original player input already
			// checked the gate (IsAttacking was true then, so TryActivate was skipped).
			UE_LOG(LogOSAttack, Log, TEXT("[COMBO] OnPhaseChanged: consuming cross-type buffer → ending self + activating %s"),
				*CrossTag.ToString());

			// Grab ASC before EndAbility (safe — instanced-per-actor ability objects persist).
			UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
				GetCurrentActivationInfo(), true, true);

			// Activate the new type. IsAttacking must be removed by now (ActivationOwnedTags
			// are synchronously cleared in EndAbility). Verify before activating — if this
			// fires, EndAbility became async or ActivationOwnedTags were misconfigured.
			if (IsValid(ASC))
			{
				if (!ensure(!ASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsAttacking)))
				{
					UE_LOG(LogOSAttack, Error,
						TEXT("[COMBO] IsAttacking still present after EndAbility — cross-type activation will fail"));
					return;
				}
				FGameplayTagContainer ActivateTags;
				ActivateTags.AddTag(CrossTag);
				ASC->TryActivateAbilitiesByTag(ActivateTags);
			}
		}
		else
		{
			UE_LOG(LogOSAttack, Log, TEXT("[COMBO] OnPhaseChanged: consuming same-type buffer → CHAIN CONTINUE (committed during Windup/Active)"));
			TryCombo(AttackTypeTag);
		}
	}
}

void UGA_OSComboAttack::OnComboInputReceived(FGameplayEventData Payload)
{
	UE_LOG(LogOSAttack, Log, TEXT("[COMBO] OnComboInputReceived: Phase=%d, IsActive=%d, InstigatorTags=%s, MyAssetTags=%s"),
		(int32)CurrentPhase, IsActive(), *Payload.InstigatorTags.ToString(), *GetAssetTags().ToString());

	// Cross-type detection: InstigatorTags carries the requested attack type from the controller
	// (e.g. Attack_Light or Attack_Heavy). We use InstigatorTags instead of EventTag because
	// WaitGameplayEvent overwrites Payload.EventTag with the routing tag.
	// MUST use HasAnyExact — hierarchical HasAny would match Attack_Light against the parent
	// Attack tag, making every input look same-type (both light and heavy are children of Attack).
	const bool bCrossType = Payload.InstigatorTags.Num() > 0 && !Payload.InstigatorTags.HasAnyExact(GetAssetTags());

	if (CurrentPhase == EOSAttackPhase::Recovery)
	{
		if (bCrossType)
		{
			// Recovery + cross-type → safe to interrupt for type-switch.
			// EndAbility removes IsAttacking (ActivationOwnedTags), allowing the controller's
			// TryActivateAbilitiesByTag to pick up the new type synchronously.
			UE_LOG(LogOSAttack, Log, TEXT("[COMBO] Cross-type switch during Recovery → ending self"));
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
				GetCurrentActivationInfo(), true, true);
			return;
		}
		// Recovery + same-type → RESTART combo from hit 0.
		// Chain continuation only happens via buffer (input during Windup/Active, consumed in
		// OnPhaseChanged). Pressing during Recovery means you waited past the commitment window
		// — you get a fresh combo, not a chain advance. This creates risk/reward: commit early
		// (during dangerous Windup/Active) for chain progression, or play safe (Recovery) for restart.
		// Clearing PreviousAttackType makes TryCombo's bSameType=false → it resets CurrentComboIndex to 0.
		UE_LOG(LogOSAttack, Log, TEXT("[COMBO] Recovery same-type → RESTART combo (waited past commitment window)"));
		PreviousAttackType = FGameplayTag();
		TryAttack(AttackTypeTag);
		return;
	}

	// Windup/Active → buffer (LIFO: last input wins, overwrites any previous buffer).
	if (bCrossType)
	{
		// Extract the requested type tag from InstigatorTags (e.g. Attack_Light).
		// This is the tag we'll pass to TryActivateAbilitiesByTag after ending self.
		const FGameplayTag RequestedTag = Payload.InstigatorTags.First();
		UE_LOG(LogOSAttack, Log, TEXT("[COMBO] Cross-type BUFFERED (Phase=%d, requested=%s, LIFO overwrite)"),
			(int32)CurrentPhase, *RequestedTag.ToString());
		SetComboBuffered(true);
		BufferedCrossTypeTag = RequestedTag;
	}
	else
	{
		// Same-type → TryAttack handles buffering via SetComboBuffered.
		TryAttack(AttackTypeTag);
	}
}

bool UGA_OSComboAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// C++ authoritative gate: block attacks while sprinting or airborne.
	// ActivationBlockedTags in the BP CDO SHOULD include these, but BP CDO serialization
	// can override the C++ constructor defaults. This override can't be bypassed by BP.
	if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(Tags.IsSprinting) || ASC->HasMatchingGameplayTag(Tags.IsInAir))
		{
			return false;
		}
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGA_OSComboAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	StoredTriggerEventData = TriggerEventData ? *TriggerEventData : FGameplayEventData();

	// CommitAbility is handled by Super (UOSGameplayAbility::ActivateAbility).
	// Do NOT call it here — double commit causes double cost deduction.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Base class commits costs and calls EndAbility on failure — bail if that happened.
	if (!IsActive()) return;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor)) return;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC)) return;

	// Cancel block if active — attack wins over block (symmetric with GA_OSBlock canceling attacks).
	{
		const FOSGameplayTags& CancelTags = FOSGameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(CancelTags.IsBlocking))
		{
			FGameplayTagContainer BlockTags;
			BlockTags.AddTag(CancelTags.Ability_Block);
			ASC->CancelAbilities(&BlockTags, nullptr, this);
		}
	}

	// WIRED: Kick off first hit. AttackTypeTag identifies this ability's type (light/heavy)
	// for TryCombo's type-switch logic. Phase is None at this point → IsPhaseValid()=true.
	UE_LOG(LogOSAttack, Log, TEXT("[COMBO] ActivateAbility: kicking off first hit, AttackTypeTag=%s"),
		*AttackTypeTag.ToString());
	TryAttack(AttackTypeTag);
}

void UGA_OSComboAttack::OnMontageSectionEnded()
{
	UE_LOG(LogOSAttack, Log, TEXT("[COMBO] OnMontageSectionEnded: Phase=%d, ending ability"),
		(int32)CurrentPhase);
	// All cleanup (tasks, buffers, state, alignment) is handled by EndAbility → CleanupComboState.
	// IsAttacking tag is auto-removed by GAS via ActivationOwnedTags when the ability ends.
	// IMPORTANT (LocalPredicted): replicate EndAbility to server so the authoritative instance clears state too.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_OSComboAttack::PerformComboHit(int32 ComboIndex)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor)) return;

	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	if (!IsValid(Character)) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC)) return;

	if (!IsValid(ComboMontage))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
		return;
	}

	// Capture old warp target name BEFORE updating index — prevents orphan targets.
	// GetWarpTargetName() depends on ActiveHitIndex; if we update first, we clear
	// the NEW name (no-op) instead of the OLD name (actual cleanup needed).
	const FName OldWarpName = GetWarpTargetName();

	// Lock the active hit index BEFORE warp injection and montage start.
	// GetWarpHitIndex() returns this — must match the index used by InjectWarpTargets.
	ActiveHitIndex = ComboIndex;

	if (!EffectiveSectionNames.IsValidIndex(ComboIndex))
	{
		UE_LOG(LogOSAttack, Error, TEXT("[COMBO] PerformComboHit: ComboIndex %d out of bounds (Num=%d) — aborting hit"),
			ComboIndex, EffectiveSectionNames.Num());
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	FName SectionToPlay = EffectiveSectionNames[ComboIndex];
	UE_LOG(LogOSAttack, Log, TEXT("[COMBO] PerformComboHit: index=%d activeHit=%d section=%s montage=%s"),
		ComboIndex, ActiveHitIndex, *SectionToPlay.ToString(), *GetNameSafe(ComboMontage.Get()));

	if (AOSCharacter* Char = Avatar())
	{
		UOSCombatBlueprintLibrary::ClearWarpTarget(Char, OldWarpName);
	}

	if (IsValid(ActiveMontageTask.Get()))
	{
		ActiveMontageTask->OnBlendOut.RemoveAll(this);
		ActiveMontageTask->OnCompleted.RemoveAll(this);
		ActiveMontageTask->OnInterrupted.RemoveAll(this);
		ActiveMontageTask->OnCancelled.RemoveAll(this);
	}
	CleanupAbilityTasks();

	ResolveAttackTarget(ComboMontage.Get());
	InjectWarpTargets(Character);

	// WIRED: Phase tracking — sets Windup, creates persistent phase event listeners.
	// Phase listeners survive across section jumps (idempotent creation in base class).
	// CleanupAbilityTasks above only clears combo-level tasks, not base class phase tasks.
	StartPhaseTracking();

	ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(*FString::Printf(TEXT("ComboHit_%d"), ComboIndex)),
		ComboMontage,
		1.0f,
		SectionToPlay,
		false,
		1.0f
	);

	if (IsValid(ActiveMontageTask.Get()))
	{
		ActiveMontageTask->OnBlendOut.AddDynamic(this, &UGA_OSComboAttack::OnMontageSectionEnded);
		ActiveMontageTask->OnCompleted.AddDynamic(this, &UGA_OSComboAttack::OnMontageSectionEnded);
		ActiveMontageTask->OnInterrupted.AddDynamic(this, &UGA_OSComboAttack::OnMontageSectionEnded);
		ActiveMontageTask->OnCancelled.AddDynamic(this, &UGA_OSComboAttack::OnMontageSectionEnded);
		ActiveMontageTask->ReadyForActivation();
	}

	// WIRED: Listen for subsequent combo presses (Event_ComboInputPressed from PlayerController).
	// Recreated per-hit because CleanupAbilityTasks destroys the previous listener.
	// OnlyTriggerOnce=false: fires on every press during this hit's lifetime.
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	if (Tags.Event_ComboInputPressed.IsValid())
	{
		ActiveWaitInputTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, Tags.Event_ComboInputPressed, nullptr, false, false);
		if (IsValid(ActiveWaitInputTask.Get()))
		{
			ActiveWaitInputTask->EventReceived.AddDynamic(this, &UGA_OSComboAttack::OnComboInputReceived);
			ActiveWaitInputTask->ReadyForActivation();
		}
	}
}

void UGA_OSComboAttack::GetEffectiveSectionNames(TArray<FName>& OutNames) const
{
	OutNames.Reset();
	if (ComboSectionNames.Num() > 0) { OutNames = ComboSectionNames; return; }
	if (IsValid(ComboMontage))
		for (int32 i = 0; i < ComboMontage->GetNumSections(); ++i)
			OutNames.Add(ComboMontage->GetSectionName(i));
}

void UGA_OSComboAttack::CleanupAbilityTasks()
{
	if (UAbilityTask_WaitGameplayEvent* Task = ActiveWaitInputTask.Get())
	{
		if (IsValid(Task) && Task->IsActive()) Task->EndTask();
		ActiveWaitInputTask = nullptr;
	}
	if (UAbilityTask_PlayMontageAndWait* Task = ActiveMontageTask.Get())
	{
		if (IsValid(Task) && Task->IsActive()) Task->EndTask();
		ActiveMontageTask = nullptr;
	}
}

void UGA_OSComboAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Save the warp name before CleanupComboState resets ActiveHitIndex.
	// GetWarpHitIndex() returns ActiveHitIndex, so we must capture before reset.
	const FName ActiveWarpName = GetWarpTargetName();
	CleanupComboState();

	if (AOSCharacter* Char = Avatar())
	{
		// Clear the MW warp target for the indexed name. Base EndAbility handles the base warp name.
		UOSCombatBlueprintLibrary::ClearWarpTarget(Char, ActiveWarpName);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_OSComboAttack::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	// Capture warp name before CleanupComboState resets ActiveHitIndex.
	const FName ActiveWarpName = GetWarpTargetName();
	CleanupComboState();

	if (AOSCharacter* Char = Avatar())
	{
		// Clear the MW warp target for the indexed name. Base OnRemoveAbility handles the base warp name.
		UOSCombatBlueprintLibrary::ClearWarpTarget(Char, ActiveWarpName);
	}

	Super::OnRemoveAbility(ActorInfo, Spec);
}

bool UGA_OSComboAttack::ApplyCostPerHit()
{
	if (CurrentComboIndex == 0) return true;
	if (CheckCost(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo()))
	{
		ApplyCost(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfoRef());
		return true;
	}
	return false;
}

void UGA_OSComboAttack::CleanupComboState()
{
	CleanupAbilityTasks();
	CurrentComboIndex = 0;
	ActiveHitIndex = 0;
	PreviousAttackType = FGameplayTag();
	// Clear BOTH the pending bool and the tag. Tag is only cleared here (EndAbility/OnRemoveAbility).
	bPendingComboInput = false;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetReplicatedLooseGameplayTagCount(FOSGameplayTags::Get().State_Attack_ComboBuffered, 0);
	}
	BufferedCrossTypeTag = FGameplayTag();
	Target = nullptr;
}





// Applies per-hit stamina cost through the full GAS pipeline (GE with negative SetByCaller magnitude).
// This ensures PostGameplayEffectExecute fires → OnAttributeStaminaChanged broadcasts → HUD updates.
// Negative magnitude convention matches base class UOSGameplayAbility::ApplyCosts (line 307: -Cost.Amount).
/*
bool UGA_OSComboAttack::ApplyStaminaCost(AActor* AvatarActor, UAbilitySystemComponent* ASC) const
{
	if (StaminaCostPerHit <= 0.0f) return true;
	const UOSAttributeSet* AS = OSAttrs();
	if (!IsValid(AS)) return false;
	const float Before = AS->GetStamina();
	if (Before < StaminaCostPerHit)
	{
		return false;
	}
	if (!IsValid(AvatarActor) || !AvatarActor->HasAuthority()) return true;
	if (IsValid(StaminaCostEffectClass))
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddSourceObject(const_cast<UGA_OSComboAttack*>(this));
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(StaminaCostEffectClass, 1.0f, EffectContext);
		if (SpecHandle.IsValid())
		{
			if (Tags.Data_Cost_Stamina.IsValid()) SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Cost_Stamina, -StaminaCostPerHit);
			if (Tags.Data_Cost_Health.IsValid()) SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Cost_Health, 0.0f);
			if (Tags.Data_Cost_Aura.IsValid()) SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Cost_Aura, 0.0f);
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
	
	return true;
}

void UGA_OSComboAttack::OnRecoveryBegin(FGameplayEventData Payload)
{
	if (!bHasComboInput) return;
	ResolveBufferedAttack(BufferedAttack.BufferedAttackType);
}



void UGA_OSComboAttack::PerformComboHit(int32 ComboIndex)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor)) return;

	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	if (!IsValid(Character)) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC)) return;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	TArray<FName> SectionNames;
	GetEffectiveSectionNames(SectionNames);
	if (!IsValid(ComboMontage) || SectionNames.Num() == 0)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
		return;
	}
	if (ComboIndex >= SectionNames.Num())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
		return;
	}

	FName SectionToPlay = SectionNames[ComboIndex];
	
	// Clear stale alignment state from previous hit (MW warp target + CMC target rotation)
	// so the two systems never fight across combo hits.
	// Uses GetWarpTargetName() which returns the indexed name for the current ComboIndex.
	if (AOSCharacter* Char = Avatar())
	{
		UOSCombatBlueprintLibrary::ClearAlignmentState(Char, GetWarpTargetName());
	}

	// Clean up old tasks BEFORE creating new ones — ending the old montage task
	// fires OnCancelled → OnMontageSectionEnded → EndAbility, which would kill
	// a just-created warp task. Unbind callbacks first to prevent that cascade.
	if (IsValid(ActiveMontageTask.Get()))
	{
		ActiveMontageTask->OnBlendOut.RemoveAll(this);
		ActiveMontageTask->OnCompleted.RemoveAll(this);
		ActiveMontageTask->OnInterrupted.RemoveAll(this);
		ActiveMontageTask->OnCancelled.RemoveAll(this);
	}
	CleanupAbilityTasks();

	// Single resolution: cone result sets alignment and Target (for warp + hit traces).
	// GetWarpHitIndex() returns CurrentComboIndex via virtual dispatch — no manual sync needed.


	// Phase tracking: start in Windup, create event listeners (persists across section jumps).
	StartPhaseTracking();

	ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(*FString::Printf(TEXT("ComboHit_%d"), ComboIndex)),
		ComboMontage,
		1.0f,
		SectionToPlay,
		false,
		1.0f
	);

	if (IsValid(ActiveMontageTask.Get()))
	{
		if (Tags.Event_ComboInputPressed.IsValid())
		{
			ActiveWaitInputTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, Tags.Event_ComboInputPressed, nullptr, false, false);
			if (IsValid(ActiveWaitInputTask.Get()))
			{
				ActiveWaitInputTask->EventReceived.AddDynamic(this, &UGA_OSComboAttack::OnComboInputReceived);
				ActiveWaitInputTask->ReadyForActivation();
			}
		}
		if (Tags.Event_Attack_Phase_Recovery.IsValid())
		{
			ActiveWaitRecoveryTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, Tags.Event_Attack_Phase_Recovery, nullptr, false, false);
			if (IsValid(ActiveWaitRecoveryTask.Get()))
			{
				ActiveWaitRecoveryTask->EventReceived.AddDynamic(this, &UGA_OSComboAttack::OnRecoveryBegin);
				ActiveWaitRecoveryTask->ReadyForActivation();
			}
		}
		
		ActiveMontageTask->OnBlendOut.AddDynamic(this, &UGA_OSComboAttack::OnMontageSectionEnded);
		ActiveMontageTask->OnCompleted.AddDynamic(this, &UGA_OSComboAttack::OnMontageSectionEnded);
		ActiveMontageTask->OnInterrupted.AddDynamic(this, &UGA_OSComboAttack::OnMontageSectionEnded);
		ActiveMontageTask->OnCancelled.AddDynamic(this, &UGA_OSComboAttack::OnMontageSectionEnded);
		ActiveMontageTask->ReadyForActivation();
	}
	OnComboHitPerformed(ComboIndex);
	// Damage is applied by montage hit windows (OSAnimNotifyState_GASAttackTrace) on the server.
}

void UGA_OSComboAttack::OnComboInputReceived(FGameplayEventData Payload)
{
	bHasComboInput = true;

	// Buffer the intent on the ASC (authoritative place for "what the player wanted").
	if (UOSAbilitySystemComponent* OSASC = Cast<UOSAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		OSASC->BufferInput(EOSBufferedInputType::ComboContinue);
	}
}

void UGA_OSComboAttack::OnComboWindowOpened(FGameplayEventData Payload)
{
}

bool UGA_OSComboAttack::TryAdvanceNow()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		return false;
	}

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	if (!ASC->HasMatchingGameplayTag(Tags.Combo_CanQueue))
	{
		return false;
	}

	TArray<FName> SectionNames;
	GetEffectiveSectionNames(SectionNames);
	const int32 EffectiveMaxHits = (MaxComboHits > 0) ? FMath::Min(MaxComboHits, SectionNames.Num()) : SectionNames.Num();

	const int32 NextIndex = CurrentComboIndex + 1;
	if (EffectiveMaxHits <= 0 || !SectionNames.IsValidIndex(NextIndex) || NextIndex >= EffectiveMaxHits)
	{
		return false;
	}

	// Consume buffered intent (best-effort; if it's expired, we just don't advance).
	if (UOSAbilitySystemComponent* OSASC = Cast<UOSAbilitySystemComponent>(ASC))
	{
		EOSBufferedInputType Consumed = EOSBufferedInputType::None;
		if (!OSASC->ConsumeBufferedInput(Consumed))
		{
			return false;
		}
	}

	// Apply stamina for the next swing.
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (StaminaCostPerHit > 0.0f && !ApplyStaminaCost(AvatarActor, ASC))
	{
		return false;
	}

	// Save previous hit's indexed warp name before incrementing — the old modifier
	// references this name, so removing it causes the engine to auto-disable the modifier.
	const FName OldWarpName = GetWarpTargetName();
	CurrentComboIndex = NextIndex;
	// GetWarpHitIndex() now returns NextIndex via virtual dispatch — no manual sync needed.

	// Clear stale alignment state from previous hit before resolving new target.
	if (AOSCharacter* Char = Avatar())
	{
		UOSCombatBlueprintLibrary::ClearAlignmentState(Char, OldWarpName);
	}

	// Resolve target and inject warp BEFORE section jump — the engine's OnBecomeRelevant
	// fires on the first frame of the warp window, so targets must exist before the jump.
	ResolveAttackTarget(ComboMontage.Get());
	InjectWarpTargets(Cast<ACharacter>(GetAvatarActorFromActorInfo()));

	// New section = new Windup. Phase listeners (created in StartPhaseTracking) persist
	// across section jumps — the ANS_OSAttackPhase on the new section will fire Active/Recovery.
	SetPhase(EOSAttackPhase::Windup);

	// HARD CUT: jump montage immediately to the next section.
	ASC->CurrentMontageJumpToSection(SectionNames[CurrentComboIndex]);

	OnComboHitPerformed(CurrentComboIndex);
	return true;
}

void UGA_OSComboAttack::ResolveBufferedAttack( FGameplayTag& tag )
{
	if (!tag.IsValid())
	{
		PerformComboHit(0);
		return;
	}
	FGameplayTagContainer& OwnTags = GetCurrentAbilitySpec()->Ability->AbilityTags;
	
	if (OwnTags.HasTag(tag))
	{
		TryAdvanceNow();
		PerformComboHit(CurrentComboIndex);
	}
	else
		PerformComboHit(0);
	
}




// Shared teardown for EndAbility and OnRemoveAbility. Centralizes cleanup that was previously
// duplicated across OnMontageSectionEnded, EndAbility, and OnRemoveAbility with subtle
// inconsistencies (e.g. ClearBufferedInput missing from OnRemoveAbility, ClearAlignmentState
// missing from OnRemoveAbility). EndAbility adds alignment cleanup on top; OnRemoveAbility
// adds Combo_CanQueue tag removal on top. IsAttacking is handled automatically by
// ActivationOwnedTags — no manual tag management needed here.


void UGA_OSComboAttack::ResetComboState()
{
	CurrentComboIndex = 0;
	bHasComboInput = false;
	StoredTriggerEventData = FGameplayEventData();
	DisableComboBuffer();
	Target = nullptr;
}



void UGA_OSComboAttack::ClearComboInput()
{
	bHasComboInput = false;
}

void UGA_OSComboAttack::EnableComboBuffer()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		ASC->AddLooseGameplayTag(FOSGameplayTags::Get().Combo_CanQueue);
}

void UGA_OSComboAttack::DisableComboBuffer()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		ASC->RemoveLooseGameplayTag(FOSGameplayTags::Get().Combo_CanQueue);
}

void UGA_OSComboAttack::PerformAttackTrace()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor)) return;

	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	if (!IsValid(Character)) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC)) return;

	FVector TraceStart = Character->GetActorLocation();
	FVector TraceDirection = Character->GetActorForwardVector();
	const AActor* EventTarget = StoredTriggerEventData.Target.Get();
	if (IsValid(EventTarget))
		TraceDirection = (EventTarget->GetActorLocation() - TraceStart).GetSafeNormal();
	else if (StoredTriggerEventData.TargetData.Num() > 0)
	{
		TArray<AActor*> TargetActors = UAbilitySystemBlueprintLibrary::GetAllActorsFromTargetData(StoredTriggerEventData.TargetData);
		if (TargetActors.Num() > 0 && IsValid(TargetActors[0]))
			TraceDirection = (TargetActors[0]->GetActorLocation() - TraceStart).GetSafeNormal();
	}
	FVector TraceEnd = TraceStart + (TraceDirection * 75.0f);
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	
	FHitResult HitResult;
	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Pawn, QueryParams))
	{
		if (AActor* HitActor = HitResult.GetActor())
		{
			if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
			{
				/*                if (IsValid(DamageEffectClass))
                {
                    const FOSGameplayTags& Tags = FOSGameplayTags::Get();
                    FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
                    EffectContext.AddSourceObject(this);
                    EffectContext.AddHitResult(HitResult);
                    const TSubclassOf<UGameplayEffect> EffectClassToUse = UOSGASUtils::SanitizeDamageEffectClass(DamageEffectClass);
                    FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClassToUse, 1.0f, EffectContext);
                    if (SpecHandle.IsValid() && Tags.Data_Damage.IsValid())
                    {
                        SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Damage, DamagePerHit);
                        ASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
                    }
                }#1#
				// NOTE: Authoritative damage is applied exclusively by `OSAnimNotifyState_GASAttackTrace` on the server.
				// Keep this trace for optional target acquisition / event dispatch only.
				if (FOSGameplayTags::Get().Event_AttackHit.IsValid())
				{
					FGameplayEventData EventData;
					EventData.Instigator = AvatarActor;
					EventData.Target = HitActor;
					// MP GAS best-practice: we already have the target ASC, so route via ASC directly.
					TargetASC->HandleGameplayEvent(FOSGameplayTags::Get().Event_AttackHit, &EventData);
				}
			}
		}
	}
}
*/
