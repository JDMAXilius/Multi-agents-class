#include "AbilitySystem/Abilities/BNMovementAbilities.h"
#include "Core/BNGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

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
