#include "AbilitySystem/Abilities/BNMovementAbilities.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/PlayerController.h"
#include "Camera/BNDashCameraShake.h"

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

	// DEBT A1 (Wave 3 critic, 27302a7): landing is the ONLY thing that ended this ability, and a
	// pawn destroyed in mid-air never lands. LandedDelegate never fires, EndAbility never runs,
	// and the Jumping/InAir GEs stay on the PERSISTENT PlayerState ASC forever — Space becomes a
	// dead key for the rest of the match, because the spec is still active on the next body.
	// Bound as the event it is rather than polled; UBNGA_Sprint::CheckGate's avatar-invalid guard
	// is the same rule on a timer. NOT left to death cancelling abilities: travel and a plain
	// DestroyPawn destroy the avatar without anyone dying.
	Character->OnDestroyed.AddDynamic(this, &UBNGA_Jump::OnAvatarDestroyed);

	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	ReleaseTask->OnRelease.AddDynamic(this, &UBNGA_Jump::OnInputRelease);
	ReleaseTask->ReadyForActivation();
}

void UBNGA_Jump::OnAvatarDestroyed(AActor* DestroyedActor)
{
	// Cancelled, not completed: nothing landed. EndAbility below removes both state GEs from the
	// ASC, which is still perfectly reachable — it is the one thing that outlived the pawn.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
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
		Character->OnDestroyed.RemoveDynamic(this, &UBNGA_Jump::OnAvatarDestroyed);
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


// ---------------------------------------------------------------------------- UBNGA_Dash

UBNGA_Dash::UBNGA_Dash()
{
	// Base defaults stand: InstancedPerActor, LocalPredicted. The press must move the body on
	// the machine that pressed it — a dodge that waits half an RTT is a dodge you did not make.
}

const FGameplayTagContainer* UBNGA_Dash::GetCooldownTags() const
{
	CooldownTags.Reset();
	CooldownTags.AddTag(BNTags::Cooldown_Dash);
	return &CooldownTags;
}

void UBNGA_Dash::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (CooldownDuration <= 0.f)
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UBNGE_DashCooldown::StaticClass(), GetAbilityLevel());
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(BNSetByCaller::DashCooldown, CooldownDuration);
	Spec.Data->DynamicGrantedTags.AddTag(BNTags::Cooldown_Dash);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

bool UBNGA_Dash::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return false;
	}

	// Not while already dashing: re-pressing mid-dash would stack launches into a slingshot.
	if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		if (ASC->HasMatchingGameplayTag(BNTags::State_Movement_Dashing))
		{
			return false;
		}
	}

	// Airborne is a CHOICE, not an accident — see bAllowInAir. Swimming/flying modes are
	// neither ground nor the air this ability means, so they are refused outright.
	if (Move->IsFalling())
	{
		return bAllowInAir;
	}
	return Move->IsMovingOnGround();
}

UAnimMontage* UBNGA_Dash::SelectDirectionalMontage(const FVector& WorldDirection, const AActor* Avatar, float& OutRollSign) const
{
	OutRollSign = 0.f;
	if (!Avatar)
	{
		return nullptr;
	}

	// THE PAWN'S OWN FRAME. A dash "left" means left of where the player is FACING, not west —
	// so the world direction is resolved against the actor's axes before anything is chosen.
	const float Forward = FVector::DotProduct(WorldDirection, Avatar->GetActorForwardVector());
	const float Right   = FVector::DotProduct(WorldDirection, Avatar->GetActorRightVector());

	// Whichever axis dominates wins. A diagonal dash is mostly-forward or mostly-sideways and
	// picking the larger component is what a player reads as correct; blending two montages
	// would be a Tier-4 anim-graph job for a 0.25s action nobody watches twice.
	const TSoftObjectPtr<UAnimMontage>& Chosen = (FMath::Abs(Forward) >= FMath::Abs(Right))
		? (Forward >= 0.f ? MontageForward : MontageBackward)
		: (Right   >= 0.f ? MontageRight   : MontageLeft);

	// The camera banks only on a LATERAL dash. Rolling on a straight forward or backward dash
	// reads as a stumble — the body tripping — rather than as a bank into the movement.
	if (FMath::Abs(Right) > FMath::Abs(Forward))
	{
		OutRollSign = Right >= 0.f ? 1.f : -1.f;
	}

	// Unset is silent and free: Tier-4 content this packet points at rather than authors, and
	// a missing montage must never cost a load attempt or a warning per press.
	return Chosen.IsNull() ? nullptr : Chosen.LoadSynchronous();
}

void UBNGA_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// THE DIRECTION, and the whole reason this is a dodge rather than a sprint. The pawn's
	// last input vector is what the player is ASKING for — strafe left, back off, cut right —
	// so the dash goes there. Flattened, because a thrust is horizontal; the Z below is
	// deliberately left to gravity.
	FVector Direction = Character->GetLastMovementInputVector();
	Direction.Z = 0.f;
	if (Direction.IsNearlyZero())
	{
		// Standing still with the key pressed: dash where they are LOOKING. The alternative —
		// refusing — makes the key feel broken at exactly the moment a player panics.
		Direction = Character->GetActorForwardVector();
		Direction.Z = 0.f;
	}
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// XY OVERRIDE, Z UNTOUCHED. Overriding horizontal makes the dash the same distance from a
	// standstill as from a sprint, which is what lets a player trust it; leaving Z alone keeps
	// it from cancelling a fall or granting height it did not earn.
	Character->LaunchCharacter(Direction * DashSpeedUU, /*bXYOverride=*/true, /*bZOverride=*/false);

	// The state tag through a GE like every other state here, so it reaches simulated proxies
	// and anything gating on "is this body dashing" reads the same answer on every machine.
	DashingHandle = ApplyStateTag(BNTags::State_Movement_Dashing);

	// THE ANIMATION, chosen by direction. Played through the ASC rather than the mesh so it
	// reaches simulated proxies — everyone watching sees the same body throw itself sideways.
	float RollSign = 0.f;
	if (UAnimMontage* Montage = SelectDirectionalMontage(Direction, Character, RollSign))
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->PlayMontage(this, ActivationInfo, Montage, 1.f);
		}
	}

	// Cosmetics on every machine, through the cue system — never spawned directly.
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		FGameplayCueParameters Params;
		Params.Location = Character->GetActorLocation();
		Params.Normal = Direction;
		Params.Instigator = Character;
		ASC->ExecuteGameplayCue(BNTags::GameplayCue_Character_Dash, Params);
	}

	// THE CAMERA AND THE HANDS, and both are LOCAL ONLY — a shake or a rumble on someone
	// else's machine is somebody else's dash being felt in your controller. IsLocalController
	// is the whole guard.
	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		if (PC->IsLocalController())
		{
			if (UClass* ShakeClass = CameraShake.IsNull() ? nullptr : CameraShake.LoadSynchronous())
			{
				// The roll SIGN rides the instance, so a left dash and a right dash are mirror
				// images rather than the same shake played twice.
				// Through the camera MANAGER, not ClientStartCameraShake: the RPC form returns
				// void, and this shake has to be reached after it starts to set its roll.
				// Safe here because the whole block is already local-only.
				UCameraShakeBase* Shake = PC->PlayerCameraManager
					? PC->PlayerCameraManager->StartCameraShake(ShakeClass)
					: nullptr;
				if (Shake)
				{
					if (UBNDashShakePattern* Pattern = Cast<UBNDashShakePattern>(Shake->GetRootShakePattern()))
					{
						Pattern->RollSign = RollSign;
					}
				}
			}

			// DYNAMIC force feedback, not an asset. It needs nothing authored, so the dash
			// has weight in the hands the first time it is pressed rather than "announced,
			// unset" like the montage and the burst. Both motors: a dash is a whole-body
			// shove, not a trigger click.
			if (HapticIntensity > 0.f && HapticDuration > 0.f)
			{
				FForceFeedbackValues Values;
				Values.LeftLarge = Values.RightLarge = HapticIntensity;
				Values.LeftSmall = Values.RightSmall = HapticIntensity * 0.6f;
				PC->PlayDynamicForceFeedback(HapticIntensity, HapticDuration,
					/*bAffectsLeftLarge=*/true, /*bAffectsLeftSmall=*/true,
					/*bAffectsRightLarge=*/true, /*bAffectsRightSmall=*/true,
					EDynamicForceFeedbackAction::Start);
			}
		}
	}

	// A TIMER, not a montage notify: nothing waits on an animation here, the ability owns its
	// own lifetime, and an unset montage must never be able to strand the dashing tag.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DashTimer,
			FTimerDelegate::CreateUObject(this, &UBNGA_Dash::EndDashWindow),
			FMath::Max(0.05f, DashDuration), /*bLoop=*/false);
		return;
	}
	EndDashWindow();
}

void UBNGA_Dash::EndDashWindow()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBNGA_Dash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// The timer first: a cancelled dash — death, a swap, the match freezing — must not have a
	// pending callback that ends an ability which has already ended.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashTimer);
	}

	// And the tag, unconditionally. A stranded State.Movement.Dashing would block every future
	// dash through CanActivateAbility above — the same never-clears shape that kept bots
	// crouched for a whole match.
	RemoveStateTag(DashingHandle);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
