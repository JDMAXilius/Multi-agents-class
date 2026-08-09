#include "GAS/Abilities/GA_OSGrab.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/OSCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Data/OSGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "GAS/Effects/GE_OSApplyDamage.h"
#include "GAS/Effects/GE_OSGrabClaim.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

#include "OSLogCategories.h"
#include "UObject/UObjectGlobals.h"

// =============================================================================
// Diagnostic logging infrastructure
// =============================================================================
//
// TWO equivalent on switches, either turns on all grab diag (logs + HUD overlay +
// existing 3D world-space grab debug via DrawGrabDebugOverlay):
//   (1) Console: OnSight.Grab.DiagLogging > 0
//   (2) Cheat menu: "Grab" checkbox — toggles GDebugGrabViz (OSDebugGlobals.h)
//
// Session ID: per-machine uint32 set on ActivateAbility, logged in every line.
// Role prefix: "Server" / "AutonomousProxy" / "SimulatedProxy" derived from actor role.
//
// Use LogOSGASGrab (not LogTemp) for grab diagnostics.
//
TAutoConsoleVariable<int32> CVarGrabDiagLogging(
	TEXT("OnSight.Grab.DiagLogging"),
	0,
	TEXT("Grab diagnostic logging: 0=off, 1+=on (logs + HUD overlay). Equivalent to the cheat-menu Grab checkbox."),
	ECVF_Cheat);

extern bool GDebugGrabViz; // from OSDebugGlobals.h — cheat-menu checkbox toggle

// Combined gate: console CVar OR cheat-menu checkbox enables all grab diag.
// Inlined at every log site so the condition can be fully optimized away when both flags are false.
static FORCEINLINE bool IsGrabDiagEnabled()
{
	return CVarGrabDiagLogging.GetValueOnAnyThread() > 0 || GDebugGrabViz;
}

static FString GrabRoleString(const AActor* Actor)
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

FName UGA_OSGrab::GetDefaultVictimPlacementWarpName()
{
	return GetDefault<UGA_OSGrab>()->VictimPlacementWarpName;
}

void UGA_OSGrab::ServerRestoreGrabVictimAfterInterrupted(AOSCharacter* Victim,
	FActiveGameplayEffectHandle* OptClaimHandle,
	const FName VictimPlacementWarpNameToClear)
{
	if (!IsValid(Victim) || !Victim->HasAuthority())
	{
		return;
	}

	if (UCapsuleComponent* VictimCapsule = Victim->GetCapsuleComponent())
	{
		VictimCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}

	if (UCharacterMovementComponent* VictimCMC = Victim->GetCharacterMovement())
	{
		VictimCMC->StopMovementImmediately();
		VictimCMC->bIgnoreClientMovementErrorChecksAndCorrection = false;
	}

	if (UAbilitySystemComponent* VictimASC = Victim->GetAbilitySystemComponent())
	{
		if (OptClaimHandle && OptClaimHandle->IsValid())
		{
			VictimASC->RemoveActiveGameplayEffect(*OptClaimHandle);
			OptClaimHandle->Invalidate();
		}
		else
		{
			FGameplayEffectQuery Query;
			Query.EffectDefinition = UGE_OSGrabClaim::StaticClass();
			VictimASC->RemoveActiveEffects(Query);
		}

		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(FOSGameplayTags::Get().Ability_GrabReaction);
		VictimASC->CancelAbilities(&CancelTags);
	}

	if (!VictimPlacementWarpNameToClear.IsNone())
	{
		UOSCombatBlueprintLibrary::ClearWarpTarget(Victim, VictimPlacementWarpNameToClear);
	}
}

UGA_OSGrab::UGA_OSGrab()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Grab);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(Tags.IsGrabbing);

	ActivationBlockedTags.AddTag(Tags.IsDead);
	ActivationBlockedTags.AddTag(Tags.IsStunned);
	ActivationBlockedTags.AddTag(Tags.IsGrabbing);
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsInAir);
	ActivationBlockedTags.AddTag(Tags.IsHitReacting);
	ActivationBlockedTags.AddTag(Tags.IsAttacking);
	ActivationBlockedTags.AddTag(Tags.IsDodging);
	ActivationBlockedTags.AddTag(Tags.IsMantling);

	// CancelAbilitiesWithTag fires once at activation — using the parent tag is safe since any
	// ability granted later isn't affected by it, only by BlockAbilitiesWithTag. Mirrors
	// GA_OSGrabReaction's pattern.
	CancelAbilitiesWithTag.AddTag(Tags.Ability);

	// BlockAbilitiesWithTag is an ongoing gate — parent would hierarchically block Ability_Death,
	// preventing the attacker's own death ability from activating mid-grab. Enumerate specific
	// siblings, omit Ability_Death (and Ability_GrabReaction — not granted to attackers anyway).
	BlockAbilitiesWithTag.AddTag(Tags.Attack);                    // covers Attack_Light + Attack_Heavy (parent tag)
	BlockAbilitiesWithTag.AddTag(Tags.Ability_ComboAttack);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_ComboAttackHeavy);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_ChargedAttack);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Block);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Shield);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_HitReaction);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Dodge);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Mantle);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Jump);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Grab);              // re-entry guard
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Recoil);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Magic_FireCone);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Magic_FrostBolt);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Melee);

	BaseCosts.Add(FOSResource{UOSAttributeSet::GetStaminaAttribute(), 20.f});

	DamageEffectClass = UGE_OSApplyDamage::StaticClass();

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_OSGrab::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Seed the per-grab session ID. Placed before any early-out so even a cost-check
	// failure log carries a session ID for grep correlation.
	GrabSessionId = FMath::Rand();

	if (IsGrabDiagEnabled())
	{
		AActor* AvatarPtr = GetAvatarActorFromActorInfo();
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag][id=0x%X] %s ActivateAbility entry"),
			GrabSessionId, *GrabRoleString(AvatarPtr));
	}

	// CRIT-4: CheckCost (not CommitAbility). Entry section plays unconditionally —
	// stamina charges only on a confirmed hit. The CheckCost gate still prevents the
	// Entry whiff animation when the player can't afford the grab.
	if (!CheckCost(Handle, ActorInfo))
	{
		if (IsGrabDiagEnabled())
		{
			UE_LOG(LogOSGASGrab, Verbose,
				TEXT("[GrabDiag][id=0x%X] %s ActivateAbility aborted — CheckCost failed (insufficient stamina)"),
				GrabSessionId, *GrabRoleString(GetAvatarActorFromActorInfo()));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AOSCharacter* Attacker = Avatar();
	if (!IsValid(Attacker))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const FOSGrabMontageSet& MontageSet = DefaultGrabMontages;

	// Validate montage BEFORE any side effects.
	if (!MontageSet.AttackerMontage)
	{
		UE_LOG(LogOSGASGrab, Warning, TEXT("[Grab] No AttackerMontage in DefaultGrabMontages."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	bDamageApplied = false;
	bConfirmTraceSucceeded = false;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	// Entry-section active-frame notify (AN_OSGrabActiveFrame) → confirm trace + hit-path setup.
	UAbilityTask_WaitGameplayEvent* ActiveFrameTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, Tags.Event_Grab_ActiveFrame);
	ActiveFrameTask->EventReceived.AddDynamic(this, &UGA_OSGrab::OnGrabActiveFrameEvent);
	ActiveFrameTask->ReadyForActivation();

	// Grab-section impact notify (AN_OSDirectDamage) → damage apply.
	UAbilityTask_WaitGameplayEvent* DamageEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, Tags.Event_DirectDamage);
	DamageEventTask->EventReceived.AddDynamic(this, &UGA_OSGrab::OnDirectDamageEvent);
	DamageEventTask->ReadyForActivation();

	// Play Entry section. On miss, Entry plays through naturally as the whiff visual.
	// On hit, OnGrabActiveFrameEvent jumps to the Grab section via ASC->CurrentMontageJumpToSection.
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageSet.AttackerMontage, 1.f, EntrySectionName, true);

	// Blend-out fallback applies damage if the impact notify never fires (e.g. interrupt mid-Grab).
	MontageTask->OnCompleted.AddDynamic(this, &UGA_OSGrab::OnGrabMontageEnd);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_OSGrab::OnGrabMontageEnd);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_OSGrab::AuthOnlyCancelAbility);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_OSGrab::AuthOnlyCancelAbility);
	MontageTask->ReadyForActivation();
}

void UGA_OSGrab::PrepareVictim(AOSCharacter* Attacker, AOSCharacter* Victim)
{
	// --- Server-authoritative state changes ---
	// StopMovement + collision disable on server only. Root motion from the victim's
	// paired montage drives the "being grabbed" trajectory — do NOT use MOVE_None
	// (it kills root motion processing on all machines including the victim's client).
	if (Attacker->HasAuthority())
	{
		if (UCharacterMovementComponent* VictimCMC = Victim->GetCharacterMovement())
		{
			VictimCMC->StopMovementImmediately();
			VictimCMC->bIgnoreClientMovementErrorChecksAndCorrection = true;
		}

		if (UCapsuleComponent* VictimCapsule = Victim->GetCapsuleComponent())
		{
			VictimCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		}
	}

	// --- Cosmetic positioning (ALL machines) ---
	// Server: authoritative placement.
	// Attacker's client: cosmetic prediction — positions ~50ms stale but close enough.
	// OnRep_ReplicatedMovement on AOSCharacter corrects simulated proxies when
	// the server's authoritative position arrives.
	const FVector ToAttacker = UOSCombatBlueprintLibrary::ComputeApproachDirection2D(Victim, Attacker);
	if (!ToAttacker.IsNearlyZero())
	{
		Victim->SetActorRotation(FRotator(0.f, ToAttacker.Rotation().Yaw, 0.f));
	}

	const float StopDist = UOSCombatBlueprintLibrary::GetCapsuleStopDistance(Attacker, Victim);
	const FTransform Placement = UOSCombatBlueprintLibrary::ComputeGrabPlacement(
		Attacker, Victim, StopDist);
	Victim->SetActorTransform(Placement, false, nullptr, ETeleportType::TeleportPhysics);

	// Set MW warp target on victim. Server sets its own local MWC; the replicated
	// pipeline below feeds the victim's owning client + third-party spectators via
	// OnRep_ReplicatedGrabVictimWarp so their MWs position smoothly during playback
	// (matches attacker pipeline's Phase 1 server-auth model).
	if (Attacker->HasAuthority())
	{
		UOSCombatBlueprintLibrary::SetupPositionWarp(Victim, VictimPlacementWarpName, Placement);

		// Phase 1: replicate the victim placement warp — third-party spectators and the
		// victim's own client consume via OnRep_ReplicatedGrabVictimWarp and apply to their
		// local MWC. Fixes audit WARN-1 (the old header comment claimed replication but
		// the warp was actually local-only on the server).
		Victim->SetReplicatedGrabVictimWarp(VictimPlacementWarpName, Placement);
	}
}

void UGA_OSGrab::InjectAttackerWarps(AOSCharacter* Attacker, AOSCharacter* Victim)
{
	if (!IsValid(Attacker) || !IsValid(Victim)) return;

	USkeletalMeshComponent* AttackerMesh = Attacker->GetMesh();
	if (!AttackerMesh) return;

	const FVector AttackerRoot = Attacker->GetActorLocation();
	const FVector VictimRoot = Victim->GetActorLocation();
	const FVector ApproachDir = (VictimRoot - AttackerRoot).GetSafeNormal2D();
	if (ApproachDir.IsNearlyZero()) return;

	const FRotator FacingRot(0.f, ApproachDir.Rotation().Yaw, 0.f);

	bool bReplicatedAttacker = false;

	for (const FOSGrabSocketPair& Pair : GrabSocketPairs)
	{
		if (Pair.AttackerWarpName.IsNone()) continue;

		float AttackerReach = 0.f;
		if (!Pair.GrabberSocket.IsNone() && AttackerMesh->DoesSocketExist(Pair.GrabberSocket))
		{
			const FVector SocketOffset = AttackerMesh->GetSocketLocation(Pair.GrabberSocket) - AttackerRoot;
			AttackerReach = FMath::Max(0.f, FVector::DotProduct(SocketOffset, Attacker->GetActorForwardVector()));
		}

		// Static warp: attacker approaches victim's position at grab initiation.
		// Deterministic on both machines — same victim position at grab time.
		FVector WarpPos = VictimRoot - ApproachDir * AttackerReach;
		WarpPos.Z = AttackerRoot.Z;

		const FTransform WarpXform(FacingRot, WarpPos);
		UOSCombatBlueprintLibrary::SetupPositionWarp(Attacker, Pair.AttackerWarpName, WarpXform);

		// Phase 1: replicate the first attacker warp target so owning client and
		// simulated proxies can apply the same warp via OnRep. Skip subsequent pairs —
		// current design replicates the approach pair only. Extend to all pairs here
		// if future grab variants add multi-pair replication needs.
		if (!bReplicatedAttacker)
		{
			Attacker->SetReplicatedGrabAttackerWarp(Pair.AttackerWarpName, WarpXform);
			bReplicatedAttacker = true;
		}
	}
}

void UGA_OSGrab::CleanupAllWarpTargets()
{
	AOSCharacter* const AttackerChar = Cast<AOSCharacter>(GetAvatarActorFromActorInfo());

	// Clean up attacker's approach warp targets on the local MWC (all machines).
	if (AttackerChar)
	{
		for (const FOSGrabSocketPair& Pair : GrabSocketPairs)
		{
			if (!Pair.AttackerWarpName.IsNone())
				UOSCombatBlueprintLibrary::ClearWarpTarget(AttackerChar, Pair.AttackerWarpName);
		}

		// Phase 1: clear replicated attacker warp flag (server only — setter is
		// HasAuthority-gated). Clients' OnRep fires with bHas=false and removes the
		// warp target locally. No-op if not on the server.
		AttackerChar->ClearReplicatedGrabAttackerWarp();
	}

	// Phase 1: victim replicated warp lives on the victim's own AOSCharacter, so clear
	// it independently of attacker avatar lifetime. On the force-removal path the
	// attacker avatar may already be null while the victim is still alive with its
	// rep flag latched — keep this guard at the top level so the victim still cleans up.
	if (AOSCharacter* VictimChar = GrabVictim.Get())
	{
		VictimChar->ClearReplicatedGrabVictimWarp();
	}

	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag][id=0x%X] %s CleanupAllWarpTargets: attackerCleared=%d victimCleared=%d"),
			GrabSessionId, *GrabRoleString(AttackerChar),
			AttackerChar ? 1 : 0,
			GrabVictim.IsValid() ? 1 : 0);
	}
}

void UGA_OSGrab::OnGrabActiveFrameEvent(FGameplayEventData Payload)
{
	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag][id=0x%X] %s AN_OSGrabActiveFrame fired (bConfirmTraceSucceeded=%d)"),
			GrabSessionId, *GrabRoleString(GetAvatarActorFromActorInfo()),
			bConfirmTraceSucceeded ? 1 : 0);
	}

	// Defense-in-depth: a duplicate active-frame notify (authoring mistake, or montage loop)
	// must not re-enter the hit-path. First success locks the flag; subsequent fires no-op.
	if (bConfirmTraceSucceeded)
	{
		return;
	}

	AOSCharacter* Attacker = Avatar();
	if (!IsValid(Attacker)) return;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	// Confirm trace runs on both server (authoritative) and owning client (LocalPredicted
	// prediction) — simulated proxies don't reach here, the ability isn't instanced on them.
	FGameplayTagContainer ExcludeTags;
	ExcludeTags.AddTag(Tags.IsDead);
	ExcludeTags.AddTag(Tags.IsGrabbed);
	ExcludeTags.AddTag(Tags.IsBeingGrabbed);
	ExcludeTags.AddTag(Tags.IsAttacking);
	ExcludeTags.AddTag(Tags.IsDodging);
	ExcludeTags.AddTag(Tags.State_Invincibility);  // CRIT-3: dodge i-frames / Downed invuln
	ExcludeTags.AddTag(Tags.IsKnockedDown);        // CRIT-3: hit-react prone targets
	ExcludeTags.AddTag(Tags.IsRecoiling);          // CRIT-3: recoiling targets

	AOSCharacter* Victim = UOSCombatBlueprintLibrary::BoxTraceForTarget(
		Attacker, ExcludeTags, GrabTraceHalfExtent, GrabTraceForwardDistance);

	if (!IsValid(Victim))
	{
		// Miss — Entry plays through naturally as the whiff visual. No commit, no victim state touched.
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag][id=0x%X] %s Trace missed. Attacker=%s — Entry plays through (whiff)"),
			GrabSessionId, *GrabRoleString(Attacker), *GetNameSafe(Attacker));
		return;
	}

	// Hit — commit now. Charges stamina on both server and owning client (LocalPredicted).
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		if (IsGrabDiagEnabled())
		{
			UE_LOG(LogOSGASGrab, Verbose,
				TEXT("[GrabDiag][id=0x%X] %s Trace hit but CommitAbility failed — ending"),
				GrabSessionId, *GrabRoleString(Attacker));
		}
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	bConfirmTraceSucceeded = true;
	GrabVictim = Victim;

	const FOSGrabMontageSet& MontageSet = DefaultGrabMontages;

	// Snap attacker to face victim (predicted on client, authoritative on server).
	const FVector ToVictim = UOSCombatBlueprintLibrary::ComputeApproachDirection2D(Attacker, Victim);
	if (!ToVictim.IsNearlyZero())
	{
		Attacker->SetActorRotation(FRotator(0.f, ToVictim.Rotation().Yaw, 0.f));
	}

	// Phase 1 preserved: owning client seeds its attacker MW warp target locally pre-auth so
	// the Grab-section MW notify has something to consume on the first tick after section jump.
	// Server's OnRep arrives mid-window and overwrites; MW redistributes the delta.
	// Simulated proxies don't run this — they get the target via OnRep only.
	// Setter's internal HasAuthority gate keeps the replicated write server-only.
	if (!Attacker->HasAuthority())
	{
		InjectAttackerWarps(Attacker, Victim);

		if (IsGrabDiagEnabled())
		{
			UE_LOG(LogOSGASGrab, Verbose,
				TEXT("[GrabDiag][id=0x%X] %s client-seeded attacker warp locally (OnRep will overwrite with server value)"),
				GrabSessionId, *GrabRoleString(Attacker));
		}
	}

	// Position victim on ALL machines. Server: authoritative (collision disable + teleport +
	// MW warp). Attacker's client: cosmetic prediction. Simulated proxies correct via
	// OnRep_ReplicatedMovement when the server's authoritative position arrives.
	PrepareVictim(Attacker, Victim);

	// Stop attacker movement on ALL machines so the autonomous proxy begins the Grab section
	// from zero velocity matching the server.
	if (UCharacterMovementComponent* AttackerCMC = Attacker->GetCharacterMovement())
	{
		AttackerCMC->StopMovementImmediately();
	}

	// === Server-authoritative hit-path setup ===
	if (Attacker->HasAuthority())
	{
		// Suppress CMC corrections during the grab. Both machines process the same root motion;
		// small timing differences cause divergence, and server corrections cause visible stutter.
		if (UCharacterMovementComponent* AttackerCMC = Attacker->GetCharacterMovement())
		{
			AttackerCMC->bIgnoreClientMovementErrorChecksAndCorrection = true;
		}

		// Phase 1: compute + replicate authoritative attacker warp target.
		InjectAttackerWarps(Attacker, Victim);

		// Force immediate replication of teleported positions to non-owning clients.
		Attacker->ForceNetUpdate();
		Victim->ForceNetUpdate();

		// --- Phase 0 primary fix: elevate net priority for the paired grab window ---
		// Snapshot defaults so EndAbility restores exactly. Both attacker and victim must be
		// boosted (two-coupled-channel replication problem). bNetBoostApplied gates the restore
		// so EndAbility skips it when the boost never fired (miss path).
		CachedAttackerNetUpdateFrequency = Attacker->GetNetUpdateFrequency();
		CachedAttackerNetPriority        = Attacker->NetPriority;
		CachedVictimNetUpdateFrequency   = Victim->GetNetUpdateFrequency();
		CachedVictimNetPriority          = Victim->NetPriority;

		Attacker->SetNetUpdateFrequency(GrabBoostedNetUpdateFrequency);
		Victim->SetNetUpdateFrequency(GrabBoostedNetUpdateFrequency);
		Attacker->NetPriority        = GrabBoostedNetPriority;
		Victim->NetPriority          = GrabBoostedNetPriority;

		bNetBoostApplied = true;

		if (IsGrabDiagEnabled())
		{
			UE_LOG(LogOSGASGrab, Verbose,
				TEXT("[GrabDiag][id=0x%X] %s NetBoost applied: AttackerFreq %.1f->%.1f VictimFreq %.1f->%.1f AttackerPrio %.2f->%.2f VictimPrio %.2f->%.2f"),
				GrabSessionId, *GrabRoleString(Attacker),
				CachedAttackerNetUpdateFrequency, GrabBoostedNetUpdateFrequency,
				CachedVictimNetUpdateFrequency, GrabBoostedNetUpdateFrequency,
				CachedAttackerNetPriority, GrabBoostedNetPriority,
				CachedVictimNetPriority, GrabBoostedNetPriority);
		}

		// Periodic ForceNetUpdate on both actors during the paired window. Elevated
		// NetUpdateFrequency alone can still be throttled under load; this guarantees a
		// minimum update cadence. Weak-pointer capture — destroyed actor = no-op on that side.
		GrabForceNetUpdateTickCount = 0;
		if (UWorld* World = Attacker->GetWorld())
		{
			TWeakObjectPtr<UGA_OSGrab> WeakSelf(this);
			TWeakObjectPtr<AOSCharacter> WeakAtk(Attacker);
			TWeakObjectPtr<AOSCharacter> WeakVic(Victim);

			FTimerDelegate TickDelegate;
			TickDelegate.BindLambda([WeakSelf, WeakAtk, WeakVic]()
			{
				AOSCharacter* A = WeakAtk.Get();
				AOSCharacter* V = WeakVic.Get();
				if (A) A->ForceNetUpdate();
				if (V) V->ForceNetUpdate();

				if (UGA_OSGrab* Self = WeakSelf.Get())
				{
					Self->GrabForceNetUpdateTickCount++;
					if (IsGrabDiagEnabled() && A)
					{
						UE_LOG(LogOSGASGrab, Verbose,
							TEXT("[GrabDiag][id=0x%X] %s ForceNetUpdate tick #%d"),
							Self->GrabSessionId, *GrabRoleString(A),
							Self->GrabForceNetUpdateTickCount);
					}
				}
			});
			World->GetTimerManager().SetTimer(
				GrabNetUpdateTimerHandle, TickDelegate, GrabForceNetUpdateInterval, /*bLoop*/true);
		}

		Attacker->ServerRegisterGrabVictimForDisconnectCleanup(Victim);

		// Claim GE applied server-only — cross-ASC predicted GE application would create
		// orphaned prediction records on the victim's ASC.
		if (UAbilitySystemComponent* AttackerASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (UAbilitySystemComponent* VictimASC = Victim->GetAbilitySystemComponent())
			{
				FGameplayEffectSpecHandle ClaimSpec = AttackerASC->MakeOutgoingSpec(
					UGE_OSGrabClaim::StaticClass(), 1.f, AttackerASC->MakeEffectContext());
				GrabClaimHandle = AttackerASC->ApplyGameplayEffectSpecToTarget(
					*ClaimSpec.Data.Get(), VictimASC);
			}
		}

		// Trigger victim's reaction ability.
		if (UAbilitySystemComponent* VictimASC = Victim->GetAbilitySystemComponent())
		{
			FGameplayEventData EventData;
			EventData.Instigator = Attacker;
			EventData.Target = Victim;
			EventData.OptionalObject = MontageSet.VictimMontage;
			VictimASC->HandleGameplayEvent(Tags.Event_GrabHit, &EventData);
		}
	}

	UE_LOG(LogOSGASGrab, Log,
		TEXT("[GrabDiag][id=0x%X] %s Grab confirmed. Attacker=%s Victim=%s AtkMontage=%s VicMontage=%s"),
		GrabSessionId, *GrabRoleString(Attacker),
		*GetNameSafe(Attacker), *GetNameSafe(Victim),
		*GetNameSafe(MontageSet.AttackerMontage), *GetNameSafe(MontageSet.VictimMontage));

	// Jump attacker montage to Grab section on both machines. The Grab section's impact-frame
	// AN_OSDirectDamage notify will later fire OnDirectDamageEvent → ApplyGrabDamage.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CurrentMontageJumpToSection(GrabSectionName);
	}
}

void UGA_OSGrab::OnDirectDamageEvent(FGameplayEventData Payload)
{
	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag][id=0x%X] %s AN_OSDirectDamage notify fired — applying grab damage"),
			GrabSessionId, *GrabRoleString(GetAvatarActorFromActorInfo()));
	}
	ApplyGrabDamage();
}

void UGA_OSGrab::OnGrabMontageEnd()
{
	// Fallback: apply damage if AN_OSDirectDamage notify was not on the montage.
	ApplyGrabDamage();

	// Server-only: the server decides when the grab ends. Its EndAbility replicates
	// to the client. Without this guard, the client's montage can blend out first
	// (frame timing differs), sending a predicted EndAbility that kills the server's
	// ability before server-side damage or cleanup runs.
	if (HasAuthority(&CurrentActivationInfo))
	{
		OSEndAbility();
	}
}

void UGA_OSGrab::ApplyGrabDamage()
{
	// CRIT-1: LocalPredicted ability — client also reaches here via OnDirectDamageEvent
	// and OnGrabMontageEnd. Cross-ASC predicted GE application on the victim creates an
	// orphan prediction record; damage must apply server-side only.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		if (IsGrabDiagEnabled())
		{
			UE_LOG(LogOSGASGrab, Verbose,
				TEXT("[GrabDiag][id=0x%X] %s ApplyGrabDamage skipped (non-authority, CRIT-1 gate)"),
				GrabSessionId, *GrabRoleString(GetAvatarActorFromActorInfo()));
		}
		return;
	}

	if (bDamageApplied)
	{
		if (IsGrabDiagEnabled())
		{
			UE_LOG(LogOSGASGrab, Verbose,
				TEXT("[GrabDiag][id=0x%X] %s ApplyGrabDamage skipped (already applied)"),
				GrabSessionId, *GrabRoleString(GetAvatarActorFromActorInfo()));
		}
		return;
	}

	AOSCharacter* Attacker = Avatar();
	AOSCharacter* Victim = GrabVictim.Get();
	if (!IsValid(Attacker) || !IsValid(Victim)) return;

	// 2D: Defense-in-depth — verify victim is still grabbed before dealing damage.
	if (UAbilitySystemComponent* VictimASC = Victim->GetAbilitySystemComponent())
	{
		if (!VictimASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsGrabbed))
		{
			UE_LOG(LogOSGASGrab, Warning,
				TEXT("[GrabDiag][id=0x%X] %s ApplyGrabDamage skipped: victim %s no longer IsGrabbed"),
				GrabSessionId, *GrabRoleString(Attacker), *GetNameSafe(Victim));
			return;
		}
	}

	bDamageApplied = true;

	if (UOSCombatBlueprintLibrary::ApplyDirectDamage(Attacker, Victim, DamageEffectClass, GrabDamage, EOSAttackType::Special))
	{
		UE_LOG(LogOSGASGrab, Log,
			TEXT("[GrabDiag][id=0x%X] %s ApplyGrabDamage applied %.0f damage to %s"),
			GrabSessionId, *GrabRoleString(Attacker), GrabDamage, *GetNameSafe(Victim));
	}
}

void UGA_OSGrab::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Clear the periodic ForceNetUpdate timer unconditionally at end. The nested restore
	// block below is gated on AvatarActor validity + HasAuthority; if the attacker pawn
	// is destroyed before EndAbility runs (disconnect mid-grab) that branch is skipped
	// and the 20Hz timer would otherwise leak on the World until level teardown.
	if (bNetBoostApplied)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(GrabNetUpdateTimerHandle);
		}
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();

	// Scoped exit branch:
	//   - Interrupt path (bWasCancelled OR !bDamageApplied): the grab didn't complete cleanly
	//     — attacker was cancelled mid-grab, or Entry whiffed, or commit failed. Victim needs
	//     full restore: re-enable collision, remove claim GE, cancel the reaction ability,
	//     clear placement warp.
	//   - Graceful path (damage applied AND not cancelled): the grab ran to completion. Victim's
	//     GA_OSGrabReaction owns its own flow end (Default → Proned → Getup). DO NOT cancel the
	//     reaction here, DO NOT remove the claim GE (the reaction removes it at Default → Proned
	//     per Task 4's Option 2 decision).
	const bool bInterruptBranch = bWasCancelled || !bDamageApplied;

	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag][id=0x%X] %s EndAbility entry bWasCancelled=%d bDamageApplied=%d branch=%s"),
			GrabSessionId, *GrabRoleString(AvatarActor),
			bWasCancelled ? 1 : 0, bDamageApplied ? 1 : 0,
			bInterruptBranch ? TEXT("interrupt") : TEXT("graceful"));
	}

	// Victim restore uses Victim->HasAuthority() so cleanup still runs when the attacker
	// avatar is already destroyed (disconnect mid-grab). No-op on non-server machines.
	if (bInterruptBranch)
	{
		if (AOSCharacter* Victim = GrabVictim.Get())
		{
			ServerRestoreGrabVictimAfterInterrupted(Victim, &GrabClaimHandle, VictimPlacementWarpName);
		}
	}

	// Restore attacker CMC + disconnect bookkeeping (requires attacker pawn).
	if (AvatarActor && AvatarActor->HasAuthority())
	{
		if (AOSCharacter* AttackerChar = Cast<AOSCharacter>(AvatarActor))
		{
			AttackerChar->ServerClearGrabVictimForDisconnectCleanup();
			if (UCharacterMovementComponent* AttackerCMC = AttackerChar->GetCharacterMovement())
			{
				AttackerCMC->bIgnoreClientMovementErrorChecksAndCorrection = false;
			}

			// --- Restore Phase 0 net-priority boost (mirrors ActivateAbility's boost block) ---
			if (bNetBoostApplied)
			{
				AttackerChar->SetNetUpdateFrequency(CachedAttackerNetUpdateFrequency);
				AttackerChar->NetPriority        = CachedAttackerNetPriority;

				if (AOSCharacter* VictimChar = GrabVictim.Get())
				{
					VictimChar->SetNetUpdateFrequency(CachedVictimNetUpdateFrequency);
					VictimChar->NetPriority        = CachedVictimNetPriority;
				}

				if (IsGrabDiagEnabled())
				{
					UE_LOG(LogOSGASGrab, Verbose,
						TEXT("[GrabDiag][id=0x%X] %s NetBoost restored: AttackerFreq=%.1f AttackerPrio=%.2f VictimFreq=%.1f VictimPrio=%.2f bWasCancelled=%d"),
						GrabSessionId, *GrabRoleString(AttackerChar),
						CachedAttackerNetUpdateFrequency, CachedAttackerNetPriority,
						CachedVictimNetUpdateFrequency, CachedVictimNetPriority,
						bWasCancelled ? 1 : 0);
				}

				bNetBoostApplied = false;
			}
		}
	}

	// Clean up attacker's MW approach warp targets.
	// MW targets are local state — safe to clean on all machines.
	CleanupAllWarpTargets();

	MontageTask = nullptr;
	GrabVictim.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_OSGrab::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	// Defense-in-depth: force-removal path (ability spec removed without EndAbility firing,
	// e.g. ApplyStartupLoadout clearing state, or character destruction during grant cleanup)
	// must still clear the timer and any replicated warp state. No-op on the normal flow
	// where EndAbility already cleared both.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GrabNetUpdateTimerHandle);
	}
	CleanupAllWarpTargets();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UGA_OSGrab::AuthOnlyCancelAbility()
{
	// CRIT-5: LocalPredicted means the owning client ticks its own montage. A client-local
	// interrupt (e.g. another ability overrides on the client but not the server) would
	// otherwise route through OSCancelAbility → CancelAbility(..., bReplicateCancelAbility=true),
	// replicating a cancel that kills a valid server grab. Only the server decides cancel.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	if (IsGrabDiagEnabled())
	{
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag][id=0x%X] %s AuthOnlyCancelAbility: server-side montage interrupt/cancel — cancelling grab"),
			GrabSessionId, *GrabRoleString(GetAvatarActorFromActorInfo()));
	}

	OSCancelAbility();
}
