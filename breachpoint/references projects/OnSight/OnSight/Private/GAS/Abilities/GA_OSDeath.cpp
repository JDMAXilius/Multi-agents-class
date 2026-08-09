// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/GA_OSDeath.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/OSCharacter.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Effects/GE_OSDeathState.h"
#include "Data/OSGameplayTags.h"
#include "Data/OSHitDamageContext.h"
#include "GameplayTagContainer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utilities/ChooserHelper.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

UGA_OSDeath::UGA_OSDeath()
{
	// Avoid FOSGameplayTags::Get() in constructor to prevent static-init crash (tag manager may not be ready).
	constexpr bool bErrorIfNotFound = false;
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Death"), bErrorIfNotFound));
	SetAssetTags(AssetTags);

	FAbilityTriggerData Trigger;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	Trigger.TriggerTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Death"), bErrorIfNotFound);
	AbilityTriggers.Add(Trigger);

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsDead"), bErrorIfNotFound));

	DeathStateEffectClass = UGE_OSDeathState::StaticClass();
}

void UGA_OSDeath::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	/* Guard: both the IsDead tag AND a local bool. Event-triggered abilities can bypass
	   ActivationBlockedTags, and multiple death events on the same frame can arrive before
	   the first activation applies the DeathState GE (so the tag check alone isn't enough). */
	/* InstancedPerActor: there is ONE instance. If this activation is a duplicate of an
	   already-running death (same-frame re-fire of Event.Death, or damage applied after
	   death GE), just drop it. Do NOT call EndAbility here: that would tear down the
	   real activation's montage/timer and leave the pawn half-dead (Death GE applied,
	   movement disabled, FinishDeath never fires, no respawn). */
	if (bDeathActivated)
		return;

	if (UAbilitySystemComponent* CheckASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (CheckASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsDead))
			return;
	}

	bDeathActivated = true;
	bFinishDeathCalled = false;

	UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] ActivateAbility on %s"), *GetNameSafe(GetOwningCharacter()));

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] CommitAbility FAILED"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UOSAbilitySystemComponent* OSASC = GetOSAbilitySystemComponent();
	if (!OSASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] No ASC found"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Grab-aware death: if victim is mid-grab when this fires, the grab reaction's Proned pose
	// IS the death visual. Let the reaction keep running and skip our own montage; administrative
	// path (DeathState GE + FinishDeath) still runs normally. Preserves Ability_GrabReaction in
	// the cancel sweep below so it isn't wiped out along with the rest of the victim's abilities.
	const FOSGameplayTags& DeathTags = FOSGameplayTags::Get();
	const bool bIsGrabbed =
		OSASC->HasMatchingGameplayTag(DeathTags.IsGrabbed) ||
		OSASC->HasMatchingGameplayTag(DeathTags.IsBeingGrabbed);

	UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] Cancelling other abilities (bIsGrabbed=%d)"), bIsGrabbed ? 1 : 0);
	if (bIsGrabbed)
	{
		FGameplayTagContainer PreserveTags;
		PreserveTags.AddTag(DeathTags.Ability_GrabReaction);
		OSASC->CancelAbilities(nullptr, &PreserveTags, this);
	}
	else
	{
		OSASC->CancelAbilities(nullptr, nullptr, this);
	}

	if (!IsActive())
	{
		UE_LOG(LogTemp, Error, TEXT("[DeathDebug] Ability was killed by CancelAbilities"));
		return;
	}

	// Apply death GE immediately so IsDead tag is granted before the montage plays.
	// This blocks all other abilities, prevents further damage, and stops movement input.
	if (DeathStateEffectClass)
	{
		FGameplayEffectContextHandle Ctx = OSASC->MakeEffectContext();
		OSASC->ApplyGameplayEffectToSelf(
			DeathStateEffectClass->GetDefaultObject<UGameplayEffect>(),
			1.0f, Ctx);
		UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] Applied DeathState GE (IsDead tag granted)"));
	}

	// Disable movement and input immediately
	if (AOSCharacter* Character = GetOwningCharacter())
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
			MoveComp->DisableMovement();
		}
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
			Character->DisableInput(PC);

		UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] Disabled movement and input"));
	}

	// Reconstruct DeathEvent (best-effort) from our custom effect context (if present)
	CachedDeathEvent = FOSDeathEventInfo();
	if (TriggerEventData)
	{
		const FGameplayEffectContextHandle& CtxHandle = TriggerEventData->ContextHandle;
		const FOSGameplayEffectContext* OSCtx = nullptr;
		if (TryGetOSGameplayEffectContext(CtxHandle, OSCtx))
		{
			auto iId = OSCtx->HitInfo.AttackInfo.InstigatorPlayerStateUniqueId;
			auto vId = OSCtx->HitInfo.AttackInfo.VictimPlayerStateUniqueId;
			CachedDeathEvent = FOSDeathEventInfo(
				OSCtx->HitInfo,
				iId,
				vId);

			CachedDeathEvent.VictimPS = CachedDeathEvent.GetVictimPlayerState(GetWorld());
			CachedDeathEvent.InstigatorPS = CachedDeathEvent.GetInstigatorPlayerState(GetWorld());
		}
	}

	// --- Direction-based montage selection ---

	UAnimMontage* MontageToPlay = DeathMontage;

	if (DeathChooser)
	{
		AActor* Avatar = GetAvatarActorFromActorInfo();

		EOSDirection Dir = EOSDirection::FRONT;
		if (TriggerEventData)
		{
			if (const FHitResult* Hit = TriggerEventData->ContextHandle.GetHitResult())
				Dir = UOSCombatBlueprintLibrary::ComputeDirection4WayFromHit(Avatar, *Hit);
		}

		/* Dir is where the hit came FROM. For death the character falls AWAY from the hit,
		   so the react direction (fall direction) is the opposite of the hit direction. */
		EOSDirection FallDir = EOSDirection::FRONT;
		switch (Dir)
		{
		case EOSDirection::FRONT: FallDir = EOSDirection::BACK;  break;
		case EOSDirection::BACK:  FallDir = EOSDirection::FRONT; break;
		case EOSDirection::LEFT:  FallDir = EOSDirection::RIGHT; break;
		case EOSDirection::RIGHT: FallDir = EOSDirection::LEFT;  break;
		default: break;
		}

		FOSHitReacts ChooserContext;
		ChooserContext.HitDirection = Dir;
		ChooserContext.ReactDirection = FallDir;
		ChooserContext.ReactType = EOSHitReactType::Death;

		if (UAnimMontage* Chosen = OSChooser::Evaluate<UAnimMontage>(DeathChooser, ChooserContext))
		{
			MontageToPlay = Chosen;
			UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] Chooser selected montage=%s Dir=%d FallDir=%d"),
				*GetNameSafe(Chosen), static_cast<int32>(Dir), static_cast<int32>(FallDir));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] Chooser returned nothing for Dir=%d, fallback=%s"),
				static_cast<int32>(Dir), *GetNameSafe(DeathMontage));
		}
	}

	// Play death montage if one is set, otherwise finish immediately.
	// Grab-aware: if the victim is mid-grab, skip the death montage entirely — the grab reaction's
	// Proned loop is the death visual. Administrative FinishDeath at the bottom of this function
	// still runs (killfeed, score, respawn queue) because it sits below the MontageToPlay branch.
	UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] MontageToPlay=%s (bIsGrabbed=%d)"),
		*GetNameSafe(MontageToPlay), bIsGrabbed ? 1 : 0);

	if (MontageToPlay && !bIsGrabbed)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			FName(TEXT("DeathMontage")),
			MontageToPlay,
			DeathMontagePlayRate,
			NAME_None,
			false,
			1.0f
		);

		if (MontageTask)
		{
			// Bind all 4 callbacks (CLAUDE.md: missing OnInterrupted is a sticky-state trap).
			// Each routes to FinishDeath; re-entry is guarded by bFinishDeathCalled.
			MontageTask->OnCompleted.AddDynamic(this,   &UGA_OSDeath::OnDeathMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this,    &UGA_OSDeath::OnDeathMontageBlendOut);
			MontageTask->OnInterrupted.AddDynamic(this, &UGA_OSDeath::OnDeathMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this,   &UGA_OSDeath::OnDeathMontageCancelled);

			MontageTask->ReadyForActivation();

			float MontageDuration = MontageToPlay->GetPlayLength() / FMath::Max(DeathMontagePlayRate, 0.1f);
			UE_LOG(LogTemp, Warning, TEXT("[DeathAnimation] MontageTask activated | Montage=%s Duration=%.2f"),
				*GetNameSafe(MontageToPlay), MontageDuration);

			// Safety-net timer in case no callback fires (mesh reinit, slot mismatch, etc.).
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(DeathTimerHandle, this, &UGA_OSDeath::FinishDeath, MontageDuration, false);
			}
			return;
		}

		UE_LOG(LogTemp, Error, TEXT("[DeathAnimation] Failed to create montage task"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] No death montage set, finishing immediately"));
	}

	FinishDeath();
}

void UGA_OSDeath::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	/* Defensive cleanup: undo the movement/input disable from ActivateAbility. If the ability
	   is cancelled before FinishDeath (e.g. by CancelAllAbilities during respawn), the pawn
	   could otherwise be left frozen. The normal death path destroys the pawn shortly after,
	   so this is mostly a safety net for cancel/interrupt flows. */
	if (AOSCharacter* Character = GetOwningCharacter())
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			if (MoveComp->MovementMode == MOVE_None)
				MoveComp->SetMovementMode(MOVE_Walking);
		}
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
			Character->EnableInput(PC);
	}

	/* Clear timer + reset activation flag BEFORE Super. InstancedPerActor means this
	   instance lives on the PlayerState's ASC and persists across pawn respawns; without
	   this reset the client's bDeathActivated stays true forever (the client's FinishDeath
	   timer often doesn't fire because the dying pawn is destroyed before MontageDuration
	   elapses), so every later death on the client is dropped as a "duplicate".
	   bFinishDeathCalled is intentionally NOT reset here: it guards re-entry of FinishDeath
	   from CancelAllAbilities->OnMontageCancelled->FinishDeath, and is reset on the next
	   ActivateAbility. */
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathTimerHandle);
	}
	bDeathActivated = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_OSDeath::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	// Defense-in-depth: force-removal (ability-set swap, spec clear, disconnect) skips
	// EndAbility. Same cleanup shape as grab's OnRemoveAbility per CL 1979/1987.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathTimerHandle);
	}
	bDeathActivated = false;
	bFinishDeathCalled = false;
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UGA_OSDeath::FinishDeath()
{
	/* Guard against re-entry: HandleDeath -> Server_Death -> CancelAllAbilities cancels this
	   ability while we're still inside FinishDeath, which fires OnDeathMontageCancelled ->
	   FinishDeath again, causing a double respawn. */
	if (bFinishDeathCalled)
		return;
	bFinishDeathCalled = true;

	UE_LOG(LogTemp, Warning, TEXT("[DeathDebug] FinishDeath on %s"), *GetNameSafe(GetOwningCharacter()));

	/* End the ability FIRST so CancelAllAbilities inside Server_Death won't re-trigger us. */
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

	if (AOSCharacter* Ch = GetOwningCharacter())
		Ch->HandleDeath(CachedDeathEvent);

	// Reset for next death after respawn (InstancedPerActor reuses the same object).
	bDeathActivated = false;
}

void UGA_OSDeath::OnDeathMontageCompleted()   { FinishDeath(); }
void UGA_OSDeath::OnDeathMontageBlendOut()    { FinishDeath(); }
void UGA_OSDeath::OnDeathMontageInterrupted() { FinishDeath(); }
void UGA_OSDeath::OnDeathMontageCancelled()   { FinishDeath(); }
