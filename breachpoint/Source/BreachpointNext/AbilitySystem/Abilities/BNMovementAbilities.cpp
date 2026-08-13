#include "AbilitySystem/Abilities/BNMovementAbilities.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

// Known gap this wave: walking off a ledge without jumping applies no State.Movement.InAir tag.

void UBNGA_Jump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The founder's verified jump: uncrouch first, then jump. IMMEDIATE, not the deferred
	// bWantsToCrouch request — CMC->UnCrouch(false) resizes the capsule now (encroachment-
	// checked, fires OnEndCrouch so the character drops the tag), so the CanJump() gate sees
	// the real answer this tick; blocked overhead leaves bIsCrouched true and the gate refuses.
	if (Character->bIsCrouched)
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->bWantsToCrouch = false;
			Move->UnCrouch(false);
		}
	}
	if (!Character->CanJump())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Character->Jump();
	JumpingHandle = ApplyStateTag(BNTags::State_Movement_Jumping);
	InAirHandle = ApplyStateTag(BNTags::State_Movement_InAir);

	Character->LandedDelegate.AddDynamic(this, &UBNGA_Jump::OnLanded);

	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	ReleaseTask->OnRelease.AddDynamic(this, &UBNGA_Jump::OnInputRelease);
	ReleaseTask->ReadyForActivation();
}

void UBNGA_Jump::OnInputRelease(float TimeHeld)
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->StopJumping();
	}
}

void UBNGA_Jump::OnLanded(const FHitResult& Hit)
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->StopJumping();
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBNGA_Jump::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		Character->LandedDelegate.RemoveDynamic(this, &UBNGA_Jump::OnLanded);
	}
	RemoveStateTag(JumpingHandle);
	RemoveStateTag(InAirHandle);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBNGA_Crouch::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The founder's verified toggle: (!bIsCrouched && !IsFalling) ? Crouch : UnCrouch.
	// The IsFalling term matters — pressing crouch mid-air UNcrouches, it never crouches.
	// The crouch tag is NOT owned here: the character's OnStartCrouch/OnEndCrouch apply and
	// remove it on the authority, off the engine's replicated crouch state.
	const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!Character->bIsCrouched && !(Move && Move->IsFalling()))
	{
		Character->Crouch();
	}
	else
	{
		Character->UnCrouch();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// The founder's gate: MyCharacter.cpp:944 refuses sprint unless ForwardAxisValue > 0, with no
// else — "sprinting sideways or backwards is not a thing." That cached axis is the owning
// client's alone. The SAME intent that the server also holds is the movement component's
// acceleration: ServerMove replicates the client's input acceleration every move, so the
// authority evaluates this from its own pawn rather than trusting a value only one machine has.
bool UBNGA_Sprint::IsGateHeld(const ACharacter* Character)
{
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return false;
	}

	const FVector Intent = Move->GetCurrentAcceleration().GetSafeNormal2D();
	return FVector::DotProduct(Intent, Character->GetActorForwardVector().GetSafeNormal2D()) > 0.1;
}

void UBNGA_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The ability is INTENT and lives until release; the gate owns the STATE. Evaluated once here
	// so the common case (W already held) applies inside the activation window, then on a timer.
	SetSprintActive(IsGateHeld(Cast<ACharacter>(ActorInfo->AvatarActor.Get())));

	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	ReleaseTask->OnRelease.AddDynamic(this, &UBNGA_Sprint::OnInputRelease);
	ReleaseTask->ReadyForActivation();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(GateTimer, this, &UBNGA_Sprint::CheckGate, 0.1f, true);
	}
}

void UBNGA_Sprint::CheckGate()
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		// The one thing that ends the ability here: sprint must not outlive its pawn on the ASC.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// Failing the gate drops the speed and the tag but keeps the ability alive, so facing forward
	// again restores sprint with no re-press. Activation is edge-triggered; this is not.
	SetSprintActive(IsGateHeld(Character));
}

void UBNGA_Sprint::SetSprintActive(bool bActive)
{
	if (bActive == bSprintApplied)
	{
		return;
	}
	bSprintApplied = bActive;

	if (bActive)
	{
		SprintingHandle = ApplyStateTag(BNTags::State_Movement_Sprinting);

		// The ONLY speed change. MaxWalkSpeed is never touched here; the GE moves the MoveSpeed
		// attribute and ABNCharacter's existing delegate carries it into the movement component.
		//
		// KNOWN, DEFERRED: MaxWalkSpeed is not part of the CMC's saved-move state, so on each
		// start/stop the owning client replays straddling moves at the current speed and sits
		// ~15-30cm ahead of the server for about one RTT. It converges and never diverges at
		// steady state; the real cure is a saved-move compressed flag (see the cmc-prediction
		// skill), which is its own packet — do not rediscover this here.
		const FGameplayEffectSpecHandle SpeedSpec = MakeOutgoingGameplayEffectSpec(UBNGE_Sprint::StaticClass(), 1.f);
		if (SpeedSpec.IsValid())
		{
			SpeedHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpeedSpec);
		}
		return;
	}

	RemoveStateTag(SprintingHandle);
	if (SpeedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveActiveGameplayEffect(SpeedHandle);
		}
		SpeedHandle = FActiveGameplayEffectHandle();
	}
}

void UBNGA_Sprint::OnInputRelease(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBNGA_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GateTimer);
	}

	SetSprintActive(false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FGameplayTag UBNGA_Lean::GetLeanTag() const
{
	return FGameplayTag();
}

void UBNGA_Lean::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FGameplayTag LeanTag = GetLeanTag();
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !LeanTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Newest side wins, and it must happen BEFORE this side's tag goes on: holding both keys
	// otherwise holds both State.Lean.* tags and the first consumer reads an ambiguous side.
	CancelOtherLeans(ActorInfo);

	LeanHandle = ApplyStateTag(LeanTag);

	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	ReleaseTask->OnRelease.AddDynamic(this, &UBNGA_Lean::OnInputRelease);
	ReleaseTask->ReadyForActivation();
}

// Cancelled, not blocked: pressing the other side should visibly lean that way, and cancelling
// runs the loser's EndAbility so its tag GE is removed rather than left held by a live ability.
// Both roles run this identically (LocalPredicted), so client and server agree on the survivor.
void UBNGA_Lean::CancelOtherLeans(const FGameplayAbilityActorInfo* ActorInfo)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			UBNGA_Lean* OtherLean = Cast<UBNGA_Lean>(Instance);
			if (OtherLean && OtherLean != this && OtherLean->IsActive())
			{
				OtherLean->CancelAbility(OtherLean->GetCurrentAbilitySpecHandle(), ActorInfo, OtherLean->GetCurrentActivationInfo(), /*bReplicateCancelAbility=*/true);
			}
		}
	}
}

void UBNGA_Lean::OnInputRelease(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBNGA_Lean::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveStateTag(LeanHandle);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FGameplayTag UBNGA_LeanLeft::GetLeanTag() const
{
	return BNTags::State_Lean_Left;
}

FGameplayTag UBNGA_LeanRight::GetLeanTag() const
{
	return BNTags::State_Lean_Right;
}
