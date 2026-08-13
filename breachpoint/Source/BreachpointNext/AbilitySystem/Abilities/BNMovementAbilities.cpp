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

	const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!IsGateHeld(Character))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SprintingHandle = ApplyStateTag(BNTags::State_Movement_Sprinting);

	// The ONLY speed change. MaxWalkSpeed is never touched here; the GE moves the MoveSpeed
	// attribute and ABNCharacter's existing delegate carries it into the movement component.
	const FGameplayEffectSpecHandle SpeedSpec = MakeOutgoingGameplayEffectSpec(UBNGE_Sprint::StaticClass(), 1.f);
	if (SpeedSpec.IsValid())
	{
		SpeedHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpeedSpec);
	}

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
	// A destroyed avatar answers false too, so sprint cannot outlive its pawn on the ASC.
	if (!IsGateHeld(Cast<ACharacter>(GetAvatarActorFromActorInfo())))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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

	RemoveStateTag(SprintingHandle);
	if (SpeedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveActiveGameplayEffect(SpeedHandle);
		}
		SpeedHandle = FActiveGameplayEffectHandle();
	}

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

	LeanHandle = ApplyStateTag(LeanTag);

	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	ReleaseTask->OnRelease.AddDynamic(this, &UBNGA_Lean::OnInputRelease);
	ReleaseTask->ReadyForActivation();
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
