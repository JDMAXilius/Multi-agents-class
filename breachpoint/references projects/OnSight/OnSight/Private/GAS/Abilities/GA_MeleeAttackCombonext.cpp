#include "GAS/Abilities/GA_MeleeAttackCombonext.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/OSCharacter.h"
#include "Data/OSGameplayTags.h"
#include "GameplayEffect.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

UGA_MeleeAttackCombonext::UGA_MeleeAttackCombonext()
{
	// GA_OSAttack sets ReplicateYes (needed for warp sync), we keep that behavior.
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

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

	if (Tags.Ability_Dodge.IsValid())
	{
		CancelAbilitiesWithTag.AddTag(Tags.Ability_Dodge);
	}
}

// ---------------------------------------------------------------------------
// ABILITY LIFECYCLE
// ---------------------------------------------------------------------------

void UGA_MeleeAttackCombonext::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
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

	// Safety net: ensure IsAttacking exists even if a BP CDO accidentally removed it.
	bAddedIsAttackingLooseTag = false;
	if (UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FGameplayTag& AttackingTag = FOSGameplayTags::Get().IsAttacking;
		if (AttackingTag.IsValid() && !AbilityASC->HasMatchingGameplayTag(AttackingTag))
		{
			AbilityASC->AddReplicatedLooseGameplayTag(AttackingTag);
			bAddedIsAttackingLooseTag = true;
		}
	}

	// Reset all combo state cleanly.
	CurrentHitIndex       = 0;
	bPendingComboInput    = false;
	bComboBufferedTagSet  = false;
	BufferedCrossTypeTag  = FGameplayTag();

	StartComboListeners();
	StartHit(0);
}

void UGA_MeleeAttackCombonext::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (bAddedIsAttackingLooseTag)
	{
		if (UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo())
		{
			const FGameplayTag& AttackingTag = FOSGameplayTags::Get().IsAttacking;
			if (AttackingTag.IsValid())
			{
				AbilityASC->RemoveReplicatedLooseGameplayTag(AttackingTag);
			}
		}
		bAddedIsAttackingLooseTag = false;
	}

	if (bComboBufferedTagSet)
	{
		if (UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo())
		{
			AbilityASC->SetReplicatedLooseGameplayTagCount(
				FOSGameplayTags::Get().State_Attack_ComboBuffered, 0);
		}
		bComboBufferedTagSet = false;
	}

	CurrentHitIndex      = 0;
	bPendingComboInput   = false;
	BufferedCrossTypeTag = FGameplayTag();

	CleanupTasks();

	// Base handles: ClearWarpTarget, SetLockedOnState(false), AttackTarget = nullptr.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MeleeAttackCombonext::OnRemoveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	CleanupTasks();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

// ---------------------------------------------------------------------------
// COMBO FLOW
// ---------------------------------------------------------------------------

void UGA_MeleeAttackCombonext::StartComboListeners()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	auto MakeTask = [this](const FGameplayTag& Tag) -> UAbilityTask_WaitGameplayEvent*
	{
		if (!Tag.IsValid()) return nullptr;
		return UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, Tag, nullptr, false, false);
	};

	WaitComboInputTask = MakeTask(Tags.Event_ComboInputPressed);
	if (IsValid(WaitComboInputTask))
	{
		WaitComboInputTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackCombonext::OnComboInputReceived);
		WaitComboInputTask->ReadyForActivation();
	}

	WaitDirectDamageTask = MakeTask(Tags.Event_DirectDamage);
	if (IsValid(WaitDirectDamageTask))
	{
		WaitDirectDamageTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackCombonext::OnDirectDamageReceived);
		WaitDirectDamageTask->ReadyForActivation();
	}
}

FName UGA_MeleeAttackCombonext::GetSectionNameForHit(int32 HitIndex) const
{
	if (ComboSectionNames.IsValidIndex(HitIndex))
	{
		return ComboSectionNames[HitIndex];
	}

	if (IsValid(ComboMontage) && HitIndex >= 0 && HitIndex < ComboMontage->GetNumSections())
	{
		return ComboMontage->GetSectionName(HitIndex);
	}

	return NAME_None;
}

void UGA_MeleeAttackCombonext::StartHit(int32 HitIndex)
{
	AOSCharacter* OwnerChar = Avatar();
	if (!IsValid(OwnerChar) || !IsValid(ComboMontage))
	{
		OSEndAbility();
		return;
	}

	const FName SectionName = GetSectionNameForHit(HitIndex);
	if (SectionName.IsNone())
	{
		// No more sections — combo chain is complete.
		OSEndAbility();
		return;
	}

	if (HitIndex > 0 && bSpendCostsPerHit)
	{
		if (!CheckCost(CurrentSpecHandle, CurrentActorInfo))
		{
			OSEndAbility();
			return;
		}
		ApplyCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	}

	// Clear previous hit's warp target BEFORE advancing the index.
	// GetWarpTargetName() is index-based via GetWarpHitIndex().
	const FName OldWarpName = GetWarpTargetName();

	CurrentHitIndex      = HitIndex;
	BufferedCrossTypeTag = FGameplayTag();

	// Clear any stale warp target from the previous hit so modifiers auto-disable cleanly.
	UOSCombatBlueprintLibrary::ClearWarpTarget(OwnerChar, OldWarpName);

	// Resolve/persist target (GA_OSAttack persists Target if still valid/in-range).
	ResolveAttackTarget(ComboMontage.Get());
	InjectWarpTargets(OwnerChar);

	if (IsValid(ActiveMontageTask) && ActiveMontageTask->IsActive())
	{
		ActiveMontageTask->EndTask();
		ActiveMontageTask = nullptr;
	}

	ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(*FString::Printf(TEXT("MeleeComboHit_%d"), HitIndex)),
		ComboMontage,
		1.f,
		SectionName,
		false,
		1.f);

	if (!IsValid(ActiveMontageTask))
	{
		OSEndAbility();
		return;
	}

	ActiveMontageTask->OnBlendOut.AddDynamic(this, &UGA_MeleeAttackCombonext::OnHitMontageEnded);
	ActiveMontageTask->OnCompleted.AddDynamic(this, &UGA_MeleeAttackCombonext::OnHitMontageEnded);
	ActiveMontageTask->OnInterrupted.AddDynamic(this, &UGA_MeleeAttackCombonext::OnHitMontageEnded);
	ActiveMontageTask->OnCancelled.AddDynamic(this, &UGA_MeleeAttackCombonext::OnHitMontageEnded);
	ActiveMontageTask->ReadyForActivation();
}

// ---------------------------------------------------------------------------
// EVENT HANDLERS
// ---------------------------------------------------------------------------

void UGA_MeleeAttackCombonext::OnComboInputReceived(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	const FGameplayTag RequestedTag = Payload.InstigatorTags.Num() > 0
		? Payload.InstigatorTags.First() : FGameplayTag();
	const FGameplayTag EffectiveTag = GetEffectiveInputTag();
	const bool bCrossType = EffectiveTag.IsValid() && RequestedTag.IsValid()
		&& !RequestedTag.MatchesTagExact(EffectiveTag);

	// Buffer the input. Cross-type or same-type — stored and consumed in OnHitMontageEnded.
	bPendingComboInput   = true;
	BufferedCrossTypeTag = bCrossType ? RequestedTag : FGameplayTag();

	if (!bComboBufferedTagSet)
	{
		if (UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo())
		{
			AbilityASC->SetReplicatedLooseGameplayTagCount(
				FOSGameplayTags::Get().State_Attack_ComboBuffered, 1);
		}
		bComboBufferedTagSet = true;
	}
}

void UGA_MeleeAttackCombonext::OnDirectDamageReceived(FGameplayEventData Payload)
{
	if (!IsActive() || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	AOSCharacter* OwnerChar = Avatar();
	if (!IsValid(OwnerChar))
	{
		return;
	}

	AOSCharacter* Victim = Cast<AOSCharacter>(GetCurrentSoftTarget());

	// Honour an explicit victim in the payload as a forward-compatible fallback.
	if (!IsValid(Victim))
	{
		if (const AOSCharacter* PayloadVictim = Cast<AOSCharacter>(Payload.Target))
		{
			if (PayloadVictim != OwnerChar)
			{
				Victim = const_cast<AOSCharacter*>(PayloadVictim);
			}
		}
	}

	if (!IsValid(Victim) || UOSCombatBlueprintLibrary::IsIncapacitated(Victim))
	{
		return;
	}

	UOSCombatBlueprintLibrary::ApplyDirectDamage(OwnerChar, Victim, DamageEffectClass, BaseDamage, AttackType);
}

void UGA_MeleeAttackCombonext::OnHitMontageEnded()
{
	if (!IsActive())
	{
		return;
	}

	// Combo advancement is server-authoritative.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	if (!bPendingComboInput)
	{
		OSEndAbility();
		return;
	}

	bPendingComboInput = false;

	// Cross-type buffered: end self and activate the requested ability.
	if (BufferedCrossTypeTag.IsValid())
	{
		const FGameplayTag SwitchTag = BufferedCrossTypeTag;
		BufferedCrossTypeTag = FGameplayTag();
		SwitchToAbilityByTag(SwitchTag);
		return;
	}

	// Same type: advance to next hit.
	StartHit(CurrentHitIndex + 1);
}

// ---------------------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------------------

FGameplayTag UGA_MeleeAttackCombonext::GetEffectiveInputTag() const
{
	if (InputActivationTag.IsValid())
	{
		return InputActivationTag;
	}

	const FOSGameplayTags& Tags            = FOSGameplayTags::Get();
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

void UGA_MeleeAttackCombonext::SwitchToAbilityByTag(const FGameplayTag& SwitchToTag)
{
	if (!SwitchToTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilityASC = GetAbilitySystemComponentFromActorInfo();

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
		GetCurrentActivationInfo(), true, true);

	if (IsValid(AbilityASC))
	{
		FGameplayTagContainer ActivateTags;
		ActivateTags.AddTag(SwitchToTag);
		AbilityASC->TryActivateAbilitiesByTag(ActivateTags);
	}
}

void UGA_MeleeAttackCombonext::CleanupTasks()
{
	auto EndIfActive = [](UAbilityTask* Task)
	{
		if (IsValid(Task) && Task->IsActive()) Task->EndTask();
	};

	EndIfActive(ActiveMontageTask);
	EndIfActive(WaitComboInputTask);
	EndIfActive(WaitDirectDamageTask);

	ActiveMontageTask    = nullptr;
	WaitComboInputTask   = nullptr;
	WaitDirectDamageTask = nullptr;
}

