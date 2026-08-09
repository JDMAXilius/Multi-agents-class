// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/GA_OSBaseMagic.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayCueNotify_Actor.h"
#include "NiagaraComponent.h"
#include "Data/OSGameplayTags.h"
#include "Data/OSHitDamageContext.h"
#include "GAS/Effects/GE_OSGenericCooldown.h"
#include "GAS/Effects/GE_OSAbilityGenericResourceCost.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "Data/OSAbilityCostAndEffects.h"

UGA_OSBaseMagic::UGA_OSBaseMagic()
	: OriginSocket(NAME_None)
	, OriginForwardOffset(100.0f)
	, bAddOffsetToSocket(false)
	, bActivateVFXImmediate(false)
	, bPreSpawnHidden(false)
	, bActivateVFXActive(false)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Default the unified cooldown GE so magic BPs only need to set CooldownDuration + CooldownTags.
	CooldownGameplayEffectClass = UGE_OSGenericCooldown::StaticClass();

	// Default the unified cost GE so magic BPs only need to populate BaseCosts (e.g., aura cost per cast).
	CostGameplayEffectClass = UGE_OSAbilityGenericResourceCost::StaticClass();

	// Per design: every magic ability costs a full aura bar (100) per cast. BPs can override by clearing
	// BaseCosts or replacing the entry if an individual ability should cost less.
	BaseCosts.Add(FOSResource{UOSAttributeSet::GetAuraAttribute(), 100.f});

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	ActivationOwnedTags.AddTag(Tags.IsAttacking);

	BlockAbilitiesWithTag.AddTag(Tags.Attack);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Dodge);

	ActivationBlockedTags.AddTag(Tags.IsDead);
	ActivationBlockedTags.AddTag(Tags.IsStunned);
	ActivationBlockedTags.AddTag(Tags.IsHitReacting);
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);
	// Block casting while mantling — magic abilities (FireCone, FrostBolt, MagicCone, etc.)
	// extend this base class, so adding IsMantling here fixes the stuck-flying bug for ALL
	// derived magic abilities at once. Previously each child ability would need its own tag add.
	ActivationBlockedTags.AddTag(Tags.IsMantling);
}

// --- Ability Lifecycle ---

void UGA_OSBaseMagic::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		OSEndAbility();
		return;
	}

	if (ACharacter* CastChar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		UE_LOG(LogTemp, Log, TEXT("[ProjectileDebug] ActivateAbility: charLoc=%s time=%.3f"),
			*CastChar->GetActorLocation().ToString(), GetWorld()->GetTimeSeconds());

		if (UCharacterMovementComponent* CMC = CastChar->GetCharacterMovement())
		{
			CMC->StopMovementImmediately();
			CMC->DisableMovement();
		}
	}

	if (!CastMontage)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] CastMontage is null. Ending."), *GetName());
		OSEndAbility();
		return;
	}

	/* VFX activation paths on ability start:
	   1. bActivateVFXImmediate=true: fire persistent cue immediately (charge-up/wind-up).
	   2. bPreSpawnHidden=true: pre-spawn the cue hidden. Revealed on the anim notify.
	   3. Otherwise: cue is fired on the anim notify in OnCastEventReceived. */
	if (ActivateVFXCueTag.IsValid() && bActivateVFXImmediate)
	{
		FireVFXCue(ActivateVFXCueTag, true);
		bActivateVFXActive = true;
	}
	else if (ActivateVFXCueTag.IsValid() && bPreSpawnHidden)
	{
		PreSpawnCueHidden(ActivateVFXCueTag);
		bActivateVFXActive = true;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, CastMontage, 1.0f);

	if (!IsValid(MontageTask))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Failed to create MontageTask"), *GetName());
		OSEndAbility();
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UGA_OSBaseMagic::OnCastMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_OSBaseMagic::OnCastMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_OSBaseMagic::OnCastMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_OSBaseMagic::OnCastMontageFinished);
	MontageTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, FOSGameplayTags::Get().Event_DirectDamage);

	if (IsValid(EventTask))
	{
		EventTask->EventReceived.AddDynamic(this, &UGA_OSBaseMagic::OnCastEventReceived);
		EventTask->ReadyForActivation();
	}
}

void UGA_OSBaseMagic::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ACharacter* CastChar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* CMC = CastChar->GetCharacterMovement())
			CMC->SetMovementMode(MOVE_Walking);
	}

	if (bActivateVFXActive && ActivateVFXCueTag.IsValid())
	{
		RemoveVFXCue(ActivateVFXCueTag);
		bActivateVFXActive = false;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// --- Event Callbacks ---

void UGA_OSBaseMagic::OnCastEventReceived(FGameplayEventData Payload)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();

	if (ActivateVFXCueTag.IsValid() && !bActivateVFXImmediate)
	{
		if (bPreSpawnHidden && bActivateVFXActive)
		{
			RevealPreSpawnedCueAtSocket(Avatar, ActivateVFXCueTag);
		}
		else if (!bActivateVFXActive)
		{
			FireVFXCue(ActivateVFXCueTag, true);
			bActivateVFXActive = true;
		}
	}

	if (CastVFXCueTag.IsValid())
		FireVFXCue(CastVFXCueTag, false);

	OnMagicEventReceived(Payload);
}

void UGA_OSBaseMagic::OnMagicEventReceived_Implementation(FGameplayEventData Payload)
{
}

void UGA_OSBaseMagic::OnCastMontageFinished()
{
	OSEndAbility();
}

// --- VFX Helpers ---

void UGA_OSBaseMagic::FireVFXCue(const FGameplayTag& CueTag, bool bPersistent)
{
	UAbilitySystemComponent* AbilityASC = ASC();
	if (!AbilityASC || !CueTag.IsValid())
		return;

	FGameplayCueParameters Params;
	Params.EffectContext = AbilityASC->MakeEffectContext();

	// Pass origin socket through the custom effect context (for C++ cues that read it directly)
	FOSGameplayEffectContext* Ctx = nullptr;
	if (TryGetOSGameplayEffectContext(Params.EffectContext, Ctx))
		Ctx->CueSocketName = OriginSocket;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	Params.Instigator = Avatar;

	if (bPersistent)
	{
		AbilityASC->AddGameplayCue(CueTag, Params);
		MoveCueActorToSocket(Avatar, CueTag);
	}
	else
	{
		AbilityASC->ExecuteGameplayCue(CueTag, Params);
	}
}

void UGA_OSBaseMagic::RemoveVFXCue(const FGameplayTag& CueTag)
{
	UAbilitySystemComponent* AbilityASC = ASC();
	if (!AbilityASC || !CueTag.IsValid())
		return;

	AbilityASC->RemoveGameplayCue(CueTag);
}

void UGA_OSBaseMagic::PreSpawnCueHidden(const FGameplayTag& CueTag)
{
	UAbilitySystemComponent* AbilityASC = ASC();
	if (!AbilityASC || !CueTag.IsValid())
		return;

	FGameplayCueParameters Params;
	Params.EffectContext = AbilityASC->MakeEffectContext();

	FOSGameplayEffectContext* Ctx = nullptr;
	if (TryGetOSGameplayEffectContext(Params.EffectContext, Ctx))
		Ctx->CueSocketName = OriginSocket;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	Params.Instigator = Avatar;

	AbilityASC->AddGameplayCue(CueTag, Params);

	AActor* CueActor = FindCueActorForTag(Avatar, CueTag);
	if (!CueActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] PreSpawnCueHidden: cue actor not found for tag %s"),
			*GetName(), *CueTag.ToString());
		return;
	}

	CueActor->SetActorHiddenInGame(true);

	TArray<UNiagaraComponent*> NiagaraComps;
	CueActor->GetComponents<UNiagaraComponent>(NiagaraComps);
	for (UNiagaraComponent* NC : NiagaraComps)
	{
		if (NC)
			NC->Deactivate();
	}
}

void UGA_OSBaseMagic::RevealPreSpawnedCueAtSocket(AActor* Avatar, const FGameplayTag& CueTag) const
{
	if (!Avatar || !CueTag.IsValid())
		return;

	AActor* CueActor = FindCueActorForTag(Avatar, CueTag);
	if (!CueActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RevealPreSpawnedCueAtSocket: cue actor not found for tag %s"),
			*GetName(), *CueTag.ToString());
		return;
	}

	const FTransform Origin = ResolveOriginTransform();
	const FVector TargetLocation = Origin.GetLocation();
	const FQuat TargetRotation = Origin.GetRotation();

	CueActor->SetActorLocationAndRotation(TargetLocation, TargetRotation);
	CueActor->SetActorHiddenInGame(false);

	ResetNiagaraAtTransform(CueActor, TargetLocation, TargetRotation);
}

void UGA_OSBaseMagic::MoveCueActorToSocket(AActor* Avatar, const FGameplayTag& CueTag) const
{
	if (!Avatar || !CueTag.IsValid())
		return;

	AActor* FoundCue = FindCueActorForTag(Avatar, CueTag);
	if (!FoundCue)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] MoveCueActorToSocket: cue actor not found for tag %s"),
			*GetName(), *CueTag.ToString());
		return;
	}

	const FTransform Origin = ResolveOriginTransform();
	const FVector TargetLocation = Origin.GetLocation();
	const FQuat TargetRotation = Origin.GetRotation();

	FoundCue->SetActorLocationAndRotation(TargetLocation, TargetRotation);

	ResetNiagaraAtTransform(FoundCue, TargetLocation, TargetRotation);
}

AActor* UGA_OSBaseMagic::FindCueActorForTag(AActor* Avatar, const FGameplayTag& CueTag) const
{
	if (!Avatar || !CueTag.IsValid())
		return nullptr;

	for (AActor* Child : Avatar->Children)
	{
		AGameplayCueNotify_Actor* CueActor = Cast<AGameplayCueNotify_Actor>(Child);
		if (!CueActor)
			continue;
		if (CueActor->GameplayCueTag != CueTag)
			continue;
		return CueActor;
	}
	return nullptr;
}

// --- Origin Resolution ---

FTransform UGA_OSBaseMagic::ResolveOriginTransform() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
		return FTransform::Identity;

	/* Rotation is always the character's actor quat. Socket bones can have baked rotations that
	   would fire projectiles/VFX sideways, and GAS default rotation for a PlayerState-owned ASC
	   is not the character's facing direction either. */
	const FQuat Rotation = Avatar->GetActorQuat();
	const FVector Forward = Avatar->GetActorForwardVector();

	// Try the socket first.
	FVector Location = Avatar->GetActorLocation();
	const FVector ActorLoc = Location;
	bool bUsedSocket = false;

	if (!OriginSocket.IsNone())
	{
		USkeletalMeshComponent* Mesh = nullptr;
		if (ACharacter* Char = Cast<ACharacter>(Avatar))
			Mesh = Char->GetMesh();
		if (!Mesh)
			Mesh = Avatar->FindComponentByClass<USkeletalMeshComponent>();

		if (Mesh && Mesh->DoesSocketExist(OriginSocket))
		{
			Location = Mesh->GetSocketLocation(OriginSocket);
			bUsedSocket = true;
			UE_LOG(LogTemp, Log, TEXT("[ProjectileDebug] ResolveOrigin: socket=%s socketLoc=%s actorLoc=%s socketDist=%.1f"),
				*OriginSocket.ToString(), *Location.ToString(), *ActorLoc.ToString(),
				FVector::Dist(Location, ActorLoc));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] ResolveOriginTransform: socket %s not found, falling back to forward offset"),
				*GetName(), *OriginSocket.ToString());
		}
	}

	// Apply forward offset when there's no socket, or when designer wants it added on top.
	if (!bUsedSocket || bAddOffsetToSocket)
	{
		Location += Forward * OriginForwardOffset;
		UE_LOG(LogTemp, Log, TEXT("[ProjectileDebug] ResolveOrigin: added forwardOffset=%.1f bAddOffsetToSocket=%d"),
			OriginForwardOffset, bAddOffsetToSocket);
	}

	UE_LOG(LogTemp, Log, TEXT("[ProjectileDebug] ResolveOrigin: finalLoc=%s totalDistFromActor=%.1f"),
		*Location.ToString(), FVector::Dist(Location, ActorLoc));

	return FTransform(Rotation, Location);
}

FVector UGA_OSBaseMagic::ResolveOriginLocation() const
{
	return ResolveOriginTransform().GetLocation();
}

void UGA_OSBaseMagic::ResetNiagaraAtTransform(AActor* CueActor, const FVector& Location, const FQuat& Rotation) const
{
	if (!CueActor)
		return;

	/* The Niagara system inside a burst cue is typically an instant emitter that spawns all particles
	   in one frame when OnActive fires. Since OnActive runs BEFORE our move (inside AddGameplayCue),
	   those particles are already emitted at the old location. We Deactivate/Activate to re-emit
	   from the new position.

	   Additionally, NiagaraComponent's bAutoManageAttachment can cancel the parent attachment on
	   Activate, leaving the component stuck at a stale world location. SetWorldLocationAndRotation
	   directly forces the component's world transform to match regardless. */
	TArray<UNiagaraComponent*> NiagaraComps;
	CueActor->GetComponents<UNiagaraComponent>(NiagaraComps);
	for (UNiagaraComponent* NC : NiagaraComps)
	{
		if (!NC)
			continue;
		NC->Deactivate();
		NC->Activate(true);
		NC->SetWorldLocationAndRotation(Location, Rotation);
	}
}
