#include "GAS/Abilities/GA_OSGrabReaction.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/OSCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Data/OSGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/Effects/GE_OSGrabClaim.h"
#include "TimerManager.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"
#include "OSLogCategories.h"

// Shared with UGA_OSGrab — single definition there, extern'd here for victim-side log parity.
extern TAutoConsoleVariable<int32> CVarGrabDiagLogging;
extern bool GDebugGrabViz; // from OSDebugGlobals.h — cheat-menu checkbox toggle

// Combined gate: console CVar OR cheat-menu checkbox enables all grab diag.
static FORCEINLINE bool IsGrabDiagEnabled()
{
	return CVarGrabDiagLogging.GetValueOnAnyThread() > 0 || GDebugGrabViz;
}

static FString GrabReactionRoleString(const AActor* Actor)
{
	if (!Actor) return TEXT("NoActor");
	switch (Actor->GetLocalRole())
	{
		case ROLE_Authority:       return TEXT("Server");
		case ROLE_AutonomousProxy: return TEXT("AutonomousProxy");
		case ROLE_SimulatedProxy:  return TEXT("SimulatedProxy");
		default:                   return TEXT("None");
	}
}

UGA_OSGrabReaction::UGA_OSGrabReaction()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_GrabReaction);
	SetAssetTags(AssetTags);

	// Event-triggered by GameplayEvent.GrabHit (same pattern as GA_OSHitReaction)
	FAbilityTriggerData Trigger;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	Trigger.TriggerTag = Tags.Event_GrabHit;
	AbilityTriggers.Add(Trigger);

	// GAS auto-manages: granted on ActivateAbility, removed on EndAbility
	ActivationOwnedTags.AddTag(Tags.IsGrabbed);

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	// Cancel victim's active abilities (combo/charged attack) to ensure clean state transition.
	// Without this, interrupted montage tasks leave orphaned tags (IsAttacking) since
	// GA_OSComboAttack doesn't bind OnInterrupted.
	// NOTE: Uses the parent Tags.Ability (hierarchical match) so this cancels ALL abilities.
	// That's fine for CancelAbilitiesWithTag because it only fires ONCE at activation — any
	// death ability triggered LATER (while grab reaction is still active) will not be cancelled
	// by this, it will only be gated by BlockAbilitiesWithTag below.
	CancelAbilitiesWithTag.AddTag(Tags.Ability);

	// Continuously block combat abilities on the victim for the duration of the grab reaction.
	//
	// CRITICAL: Do NOT use Tags.Ability (parent) here — GAS tag matching is hierarchical, so
	// Gameplay.Ability would also match Gameplay.Ability.Death and block GA_OSDeath from
	// activating when grab damage reduces HP to 0. That silently breaks grab-kills: victim
	// sits at 0 HP, fully alive, until something non-grab (e.g. combo attack) triggers death
	// after the grab releases. Explicitly enumerate child ability tags and omit Ability_Death
	// so the death trigger can always fire.
	//
	// Also omitted:
	//   - Ability_GrabReaction (self — would conflict with re-activation guard below)
	BlockAbilitiesWithTag.AddTag(Tags.Attack);                   // covers Attack_Light + Attack_Heavy (parent tag)
	BlockAbilitiesWithTag.AddTag(Tags.Ability_ComboAttack);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_ComboAttackHeavy);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_ChargedAttack);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Block);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Shield);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_HitReaction);      // grab should override hit-react
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Dodge);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Mantle);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Jump);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Grab);             // prevent grabbed victim from grabbing back
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Recoil);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Magic_FireCone);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Magic_FrostBolt);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Melee);

	// Block re-activation while already grabbed. Prevents double/triple-fire from
	// HandleGameplayEvent + server replication in same-process PIE.
	// ActivationOwnedTags grants IsGrabbed on first activate; this blocks subsequent attempts.
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);

	// Only block on death (from base class). Grab should override hit-react.
}

void UGA_OSGrabReaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrabReaction, Verbose,
			TEXT("[GrabDiag] %s GrabReaction ActivateAbility entry"),
			*GrabReactionRoleString(GetAvatarActorFromActorInfo()));
	}

	// IsGrabbed tag managed by ActivationOwnedTags (auto-granted on activate, removed on end).

	AOSCharacter* VictimChar = Avatar();
	if (!IsValid(VictimChar))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Resolve attacker once — used for rotation snap and local teleport below.
	// On the victim's client the attacker is a simulated proxy, but its position
	// is close enough for placement computation.
	AOSCharacter* AttackerChar = nullptr;
	if (TriggerEventData)
	{
		AttackerChar = Cast<AOSCharacter>(const_cast<AActor*>(TriggerEventData->Instigator.Get()));
	}

	// Stop victim movement immediately on ALL machines. This ability is ServerInitiated +
	// ReplicateYes, so this runs on both server and the victim's client. Critical for
	// preventing velocity carry-over during the latency window before the montage starts.
	if (UCharacterMovementComponent* CMC = VictimChar->GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
	}

	// Snap victim to face attacker on the victim's own machine.
	// Server already did this in PrepareVictim, but the victim's client hasn't.
	if (IsValid(AttackerChar))
	{
		const FVector ToAttacker = (AttackerChar->GetActorLocation() - VictimChar->GetActorLocation()).GetSafeNormal2D();
		if (!ToAttacker.IsNearlyZero())
		{
			VictimChar->SetActorRotation(FRotator(0.f, ToAttacker.Rotation().Yaw, 0.f));
		}
	}

	// Disable pawn collision on victim's client. Server does this in PrepareVictim,
	// but the client's simulated proxy of the victim doesn't receive collision changes.
	if (UCapsuleComponent* Capsule = VictimChar->GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	// Local teleport: compute the same placement as GA_OSGrab::PrepareVictim and apply.
	// On the server this is redundant (PrepareVictim already teleported), but harmless.
	// On the victim's client this eliminates the 50-100ms CMC replication lag snap —
	// the victim is at the correct position BEFORE the montage starts playing.
	if (IsValid(AttackerChar))
	{
		const float StopDist = UOSCombatBlueprintLibrary::GetCapsuleStopDistance(AttackerChar, VictimChar);
		const FTransform Placement = UOSCombatBlueprintLibrary::ComputeGrabPlacement(
			AttackerChar, VictimChar, StopDist);
		VictimChar->SetActorTransform(Placement, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Receive victim montage from attacker's paired set via OptionalObject
	UAnimMontage* SelectedMontage = nullptr;
	if (TriggerEventData && TriggerEventData->OptionalObject)
	{
		SelectedMontage = Cast<UAnimMontage>(const_cast<UObject*>(TriggerEventData->OptionalObject.Get()));
	}
	if (!SelectedMontage)
	{
		SelectedMontage = FallbackVictimMontage;
	}

	if (!SelectedMontage)
	{
		UE_LOG(LogOSGASGrabReaction, Warning, TEXT("[GrabReaction] No victim montage from attacker and no FallbackVictimMontage set."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Cache for the Client RPC — autonomous-proxy owning client uses this to target the correct
	// montage when jumping to Getup (OnRep's IsLocallyControlled gate means server's replicated
	// jump doesn't reach this client via RepAnimMontageInfo).
	CachedMontage = SelectedMontage;

	// Probe section structure: full flow requires Default + Proned + Getup sections to exist.
	// If any is missing (e.g. fallback montage AM_Hit_Finisher_14_Retargeted), fall back to the
	// pre-Phase-2a "play once, end on blend-out" behavior. Graceful degradation.
	const int32 DefaultIdx = SelectedMontage->GetSectionIndex(DefaultSectionName);
	const int32 PronedIdx  = SelectedMontage->GetSectionIndex(PronedSectionName);
	const int32 GetupIdx   = SelectedMontage->GetSectionIndex(GetupSectionName);
	bSectionedMontage = (DefaultIdx != INDEX_NONE) && (PronedIdx != INDEX_NONE) && (GetupIdx != INDEX_NONE);

	UE_LOG(LogOSGASGrabReaction, Log,
		TEXT("[GrabDiag] %s GrabReaction activated. Victim=%s Montage=%s bSectioned=%d"),
		*GrabReactionRoleString(GetAvatarActorFromActorInfo()),
		*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(SelectedMontage),
		bSectionedMontage ? 1 : 0);

	if (!bSectionedMontage)
	{
		UE_LOG(LogOSGASGrabReaction, Warning,
			TEXT("[GrabDiag] %s GrabReaction: montage %s lacks Default/Proned/Getup sections (%d/%d/%d) — "
			     "falling back to pre-Phase-2a behavior (play once, end on blend-out)"),
			*GrabReactionRoleString(GetAvatarActorFromActorInfo()), *GetNameSafe(SelectedMontage),
			DefaultIdx, PronedIdx, GetupIdx);
	}

	// Sectioned path plays from Default section explicitly; fallback plays from first section.
	const FName StartSection = bSectionedMontage ? DefaultSectionName : NAME_None;

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, SelectedMontage, 1.f, StartSection, true);

	// Bind all four montage callbacks per CLAUDE.md — missing OnInterrupted causes permanent
	// stuck ability state when another montage overrides.
	MontageTask->OnCompleted.AddDynamic(this, &UGA_OSGrabReaction::OnReactionMontageEnd);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_OSGrabReaction::OnReactionMontageEnd);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_OSGrabReaction::OSCancelAbility);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_OSGrabReaction::OSCancelAbility);
	MontageTask->ReadyForActivation();

	// Sectioned-path section linking — the Default→Proned transition is the ONLY section link
	// we configure at runtime; Proned→Proned self-loop and Getup→None are asset-authored in
	// AM_Brawler-Grab-Victim and don't need runtime overrides.
	//
	// Two sources of truth drive montage sections on the victim:
	//   - Autonomous proxy (owning victim client): plays its own local UAbilityTask_PlayMontageAndWait.
	//     UE 5.6 OnRep_ReplicatedAnimMontage skips section reconciliation via !IsLocallyControlled
	//     gate (AbilitySystemComponent_Abilities.cpp:3274). Needs local AnimInstance wiring.
	//   - Simulated proxy (remote observer): follows server's RepAnimMontageInfo. Needs server's
	//     authoritative section config to match local wiring so OnRep doesn't overwrite it.
	//
	// Hybrid fix: AnimInstance->Montage_SetNextSection runs on all machines (owning client's local
	// wiring). Server ALSO calls ASC->CurrentMontageSetNextSectionName to update RepAnimMontageInfo
	// so simulated-proxy OnRep sees matching NextSectionID and leaves local wiring intact.
	if (bSectionedMontage && IsValid(VictimChar))
	{
		if (USkeletalMeshComponent* Mesh = VictimChar->GetMesh())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				// Local wiring — effective for autonomous proxy (owning client self-view).
				AnimInstance->Montage_SetNextSection(DefaultSectionName, PronedSectionName, SelectedMontage);

				if (IsGrabDiagEnabled())
				{
					UE_LOG(LogOSGASGrabReaction, Verbose,
						TEXT("[GrabDiag] %s GrabReaction: wired local section link Default→Proned (asset handles Proned self-loop + Getup→None)"),
						*GrabReactionRoleString(VictimChar));
				}
			}
		}

		// Server-authoritative replication — writes RepAnimMontageInfo.NextSectionID so simulated
		// proxies' OnRep sees a matching target and leaves their local wiring alone.
		if (VictimChar->HasAuthority())
		{
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
			{
				ASC->CurrentMontageSetNextSectionName(DefaultSectionName, PronedSectionName);

				if (IsGrabDiagEnabled())
				{
					UE_LOG(LogOSGASGrabReaction, Verbose,
						TEXT("[GrabDiag] %s GrabReaction: ASC->CurrentMontageSetNextSectionName(Default, Proned) — replicates via RepAnimMontageInfo"),
						*GrabReactionRoleString(VictimChar));
				}
			}
		}

		// Timer is server-authoritative — it drives the one-time logic (apply GE, remove claim,
		// start ProneDurationAlive). Client-side reaction instances observe state changes via
		// replicated ability tag + GE state, not by running their own timers.
		if (VictimChar->HasAuthority())
		{
			const float DefaultLength = SelectedMontage->GetSectionLength(DefaultIdx);

			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					DefaultSectionTimerHandle,
					FTimerDelegate::CreateUObject(this, &UGA_OSGrabReaction::OnDefaultSectionEnd),
					DefaultLength,
					false);

				if (IsGrabDiagEnabled())
				{
					UE_LOG(LogOSGASGrabReaction, Verbose,
						TEXT("[GrabDiag] %s GrabReaction: Default timer started (length=%.2fs, fires at section-end boundary)"),
						*GrabReactionRoleString(VictimChar), DefaultLength);
				}
			}
		}
	}
}

void UGA_OSGrabReaction::OnDefaultSectionEnd()
{
	// Server-only transition: Default section's authored length has elapsed. Apply downed state,
	// transfer claim ownership from attacker to reaction, jump montage to Proned, start the
	// ProneDurationAlive timer for alive-path Getup.
	AOSCharacter* Victim = Cast<AOSCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Victim) || !Victim->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrabReaction, Verbose,
			TEXT("[GrabDiag] %s GrabReaction: Default→Proned logic trigger — applying DownedEffect, removing GrabClaim, starting ProneDurationAlive timer (section link handles visual transition)"),
			*GrabReactionRoleString(Victim));
	}

	// Apply DownedEffect (grants IsDowned + Invincibility). Handle cached for atomic removal in EndAbility.
	if (DownedEffect)
	{
		const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(DownedEffect, 1.f, Context);
		if (Spec.IsValid())
		{
			DownedEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	else
	{
		UE_LOG(LogOSGASGrabReaction, Warning,
			TEXT("[GrabDiag] %s GrabReaction: DownedEffect UPROPERTY unset — victim will not receive IsDowned/Invincibility tags"),
			*GrabReactionRoleString(Victim));
	}

	// Remove GrabClaim — Phase 2a Option 2 transfers ownership from attacker to reaction at this
	// transition. Attacker's graceful EndAbility will no longer touch the claim (Task 3).
	FGameplayEffectQuery ClaimQuery;
	ClaimQuery.EffectDefinition = UGE_OSGrabClaim::StaticClass();
	ASC->RemoveActiveEffects(ClaimQuery);

	// Visual transition Default→Proned is driven by the runtime section link wired in
	// ActivateAbility (CurrentMontageSetNextSectionName(Default, Proned)). No explicit jump
	// needed here — calling one would be a no-op or redundant since the montage is already
	// transitioning via the link.

	// Start the ProneDurationAlive timer. Alive path: timer fires → jump to Getup. Dead path:
	// timer fires → no-op (Proned self-loops until pawn destroy via respawn pipeline).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ProneTimerHandle,
			FTimerDelegate::CreateUObject(this, &UGA_OSGrabReaction::OnProneTimerEnd),
			ProneDurationAlive,
			false);
	}
}

void UGA_OSGrabReaction::OnProneTimerEnd()
{
	// Server-only: decide alive vs dead path.
	AOSCharacter* Victim = Cast<AOSCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Victim) || !Victim->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	const bool bIsDead = ASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsDead);

	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrabReaction, Verbose,
			TEXT("[GrabDiag] %s GrabReaction: Prone timer end (bIsDead=%d) — %s"),
			*GrabReactionRoleString(Victim), bIsDead ? 1 : 0,
			bIsDead ? TEXT("dead path, Proned keeps looping until respawn") : TEXT("alive path, jumping to Getup"));
	}

	if (bIsDead)
	{
		// Dead path: Proned loops until respawn pipeline destroys pawn → OnRemoveAbility → EndAbility.
		return;
	}

	// Alive path: explicit jump to Getup. Server's jump replicates to simulated proxies via
	// RepAnimMontageInfo (OnRep_ReplicatedAnimMontage applies section state for non-locally-controlled).
	ASC->CurrentMontageJumpToSection(GetupSectionName);

	// Autonomous proxy (owning victim client) skips the OnRep section reconciliation
	// (UE 5.6 IsLocallyControlled gate), so server's replicated jump doesn't reach them.
	// Client Reliable RPC forces the owning client's local AnimInstance to jump directly.
	ClientJumpToGetupSection();

	// OnReactionMontageEnd fires when Getup blends out → EndAbility.
}

void UGA_OSGrabReaction::ClientJumpToGetupSection_Implementation()
{
	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrabReaction, Verbose,
			TEXT("[GrabDiag] %s GrabReaction: ClientJumpToGetupSection RPC received — jumping local montage to %s"),
			*GrabReactionRoleString(GetAvatarActorFromActorInfo()), *GetupSectionName.ToString());
	}

	AOSCharacter* Victim = Cast<AOSCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Victim))
	{
		return;
	}

	UAnimMontage* Montage = CachedMontage.Get();
	if (!Montage)
	{
		// Pawn or montage GC'd before RPC arrived — unusual but possible under late-join / disconnect.
		return;
	}

	if (USkeletalMeshComponent* Mesh = Victim->GetMesh())
	{
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			// Local-only section jump — avoid ASC->CurrentMontageJumpToSection since that would
			// route through ServerCurrentMontageJumpToSectionName (client RPC back to server),
			// which is redundant since the server-side ability already jumped.
			AnimInstance->Montage_JumpToSection(GetupSectionName, Montage);
		}
	}
}

void UGA_OSGrabReaction::OnReactionMontageEnd()
{
	// Natural montage end (Getup complete on alive path, or fallback montage play-through).
	// Does NOT fire mid-montage during section jumps — those don't transition the montage
	// task's playing/blend-out state.
	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrabReaction, Verbose,
			TEXT("[GrabDiag] %s GrabReaction: montage end — ending ability normally"),
			*GrabReactionRoleString(GetAvatarActorFromActorInfo()));
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_OSGrabReaction::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrabReaction, Verbose,
			TEXT("[GrabDiag] %s GrabReaction EndAbility entry bWasCancelled=%d bSectioned=%d"),
			*GrabReactionRoleString(GetAvatarActorFromActorInfo()),
			bWasCancelled ? 1 : 0, bSectionedMontage ? 1 : 0);
	}

	// Clear timers unconditionally. Safe when timers never fired (miss/fallback path) —
	// ClearTimer on an inactive handle is a no-op.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DefaultSectionTimerHandle);
		World->GetTimerManager().ClearTimer(ProneTimerHandle);
	}

	// Remove DownedEffect atomically — single handle clears IsDowned + Invincibility. Matches
	// the user preference for GE-based state management over loose-tag bookkeeping.
	if (DownedEffectHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveActiveGameplayEffect(DownedEffectHandle);
		}
		DownedEffectHandle.Invalidate();
	}

	// Safety-net GrabClaim removal (server-only). Task 3 moved claim cleanup from the attacker's
	// graceful EndAbility into the reaction's ownership (Option 2). The reaction's OnDefaultSectionEnd
	// is the intended removal site, but it only runs on the sectioned path AND requires winning a
	// timing race against the Default-end blend-out. Covering every exit here (including fallback,
	// early interrupt, pawn destroy) prevents the claim leaking and leaving IsBeingGrabbed stuck on
	// the victim — which would otherwise make the victim untargetable for follow-up grabs.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (AActor* Avatar = GetAvatarActorFromActorInfo(); Avatar && Avatar->HasAuthority())
		{
			FGameplayEffectQuery ClaimQuery;
			ClaimQuery.EffectDefinition = UGE_OSGrabClaim::StaticClass();
			ASC->RemoveActiveEffects(ClaimQuery);
		}
	}

	// Safety net: restore pawn collision regardless of how we ended.
	// Primary path is UGA_OSGrab::ServerRestoreGrabVictimAfterInterrupted, but if the attacker dies mid-grab
	// or the ability is cancelled externally, the victim must not stay ECR_Ignore forever.
	if (AOSCharacter* Victim = Cast<AOSCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCapsuleComponent* Capsule = Victim->GetCapsuleComponent())
		{
			if (Capsule->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore)
			{
				Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			}
		}
	}

	MontageTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
