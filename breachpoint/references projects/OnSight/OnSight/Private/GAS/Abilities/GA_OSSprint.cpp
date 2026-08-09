// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/GA_OSSprint.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "GAS/Effects/GE_OSSprintState.h"
#include "Data/OSGameplayTags.h"
#include "Characters/OSCharacter.h"
#include "Characters/Player/OSPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "TimerManager.h"

UGA_OSSprint::UGA_OSSprint()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Sprint);
	SetAssetTags(AssetTags);

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	// Sprint does not cancel other abilities by default (toggle locomotion). Override in BP if you want Jump-style cancels.
	CancelAbilitiesWithTag.Reset();
	BlockAbilitiesWithTag.Reset();

	// IsDead already on UOSGameplayAbility. Align with GA_OSJump for incapacitated / exclusive locomotion.
	ActivationBlockedTags.AddTag(Tags.IsStunned);
	ActivationBlockedTags.AddTag(Tags.IsBlocking);
	ActivationBlockedTags.AddTag(Tags.IsAttacking);
	ActivationBlockedTags.AddTag(Tags.IsHitReacting);
	ActivationBlockedTags.AddTag(Tags.IsRecoiling);
	ActivationBlockedTags.AddTag(Tags.IsKnockedDown);
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsBeingGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsMantling);
	ActivationBlockedTags.AddTag(Tags.IsGrabbing);
	ActivationBlockedTags.AddTag(Tags.IsCharging);
	ActivationBlockedTags.AddTag(Tags.State_IsStunLock);
	DrainCheckInterval = 0.1f;
	SprintStateEffectHandle.Invalidate();

	SprintStateEffectClass = UGE_OSSprintState::StaticClass();
}

bool UGA_OSSprint::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayTagContainer* SourceTags, 
	const FGameplayTagContainer* TargetTags, 
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	return IsCharacterMoving() && HasStaminaToSprint();
}

void UGA_OSSprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, 
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return; // Super may EndAbility on commit failure

	UOSAbilitySystemComponent* ASC = GetOSAbilitySystemComponent();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplySprintStateEffect();
	ApplyPeriodicResourceDrain();
	StartSprintCheckLoop();

	// Register tag-change listener: cancel sprint the instant IsInAir appears.
	// Reactive (not polled) — fires immediately when GE_OSInAirState grants the tag.
	if (UAbilitySystemComponent* SprintASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FGameplayTag& InAirTag = FOSGameplayTags::Get().IsInAir;
		if (InAirTag.IsValid())
		{
			InAirTagDelegateHandle = SprintASC->RegisterGameplayTagEvent(InAirTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &UGA_OSSprint::OnInAirTagChanged);
		}
	}

	// GAS best practice: Update camera distance when sprint activates
	//UpdateCameraDistance(SprintCameraDistance);
}

void UGA_OSSprint::EndAbility(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, 
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Unregister InAir tag listener (must happen before removing effects, in case removal triggers tag changes).
	if (InAirTagDelegateHandle.IsValid())
	{
		if (UAbilitySystemComponent* SprintASC = GetAbilitySystemComponentFromActorInfo())
		{
			const FGameplayTag& InAirTag = FOSGameplayTags::Get().IsInAir;
			SprintASC->RegisterGameplayTagEvent(InAirTag, EGameplayTagEventType::NewOrRemoved)
				.Remove(InAirTagDelegateHandle);
		}
		InAirTagDelegateHandle.Reset();
	}

	RemoveSprintStateEffect();
	RemovePeriodicResourceDrain();

	// Clear camera interpolation timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CameraInterpTimerHandle);
	}
	
	// GAS best practice: Restore default camera distance when sprint ends
	//UpdateCameraDistance(DefaultCameraDistance);
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_OSSprint::StartSprintCheckLoop()
{
	if (UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, DrainCheckInterval))
	{
		WaitTask->OnFinish.AddDynamic(this, &UGA_OSSprint::OnSprintCheck);
		WaitTask->ReadyForActivation();
	}
}

void UGA_OSSprint::OnSprintCheck()
{
	if (!IsCharacterMoving() || !HasStaminaToSprint())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		return;
	}
	
	StartSprintCheckLoop();
}

void UGA_OSSprint::ApplySprintStateEffect()
{
	UOSAbilitySystemComponent* ASC = GetOSAbilitySystemComponent();
	if (!ASC || SprintStateEffectHandle.IsValid() || !SprintStateEffectClass)
	{
		return;
	}
	
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(SprintStateEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	// Drive sprint multiplier through SetByCaller (authoritative & predictable).
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Movement_SpeedMultiplier, SprintSpeedMultiplier);

	SprintStateEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UGA_OSSprint::RemoveSprintStateEffect()
{
	UOSAbilitySystemComponent* ASC = GetOSAbilitySystemComponent();
	if (!ASC)
	{
		SprintStateEffectHandle.Invalidate();
		return;
	}
	
	if (SprintStateEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(SprintStateEffectHandle);
		SprintStateEffectHandle.Invalidate();
		return;
	}
	
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	FGameplayTagContainer EffectTags;
	EffectTags.AddTag(Tags.Effect_Movement_Sprint);
	ASC->RemoveActiveEffectsWithGrantedTags(EffectTags);
}

bool UGA_OSSprint::IsCharacterMoving() const
{
	AOSCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return false;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		return false;
	}

	return MoveComp->Velocity.SizeSquared2D() > MinVelocityThresholdSquared;
}

bool UGA_OSSprint::HasStaminaToSprint() const
{
	UOSAttributeSet* AttrSet = OSAttrs();
	return AttrSet && AttrSet->GetStamina() > 0.0f;
}

/*void UGA_OSSprint::UpdateCameraDistance(float TargetDistance)
{
	AOSCharacter* Character = GetOwningCharacter();
	if (!IsValid(Character)) return;

	// Try to get OSPlayer for camera access
	if (AOSPlayer* Player = Cast<AOSPlayer>(Character))
	{
		USpringArmComponent* CameraBoom = Player->GetCameraBoom();
		if (IsValid(CameraBoom))
		{
			// Store current distance for interpolation
			float CurrentDistance = CameraBoom->TargetArmLength;
			
			// Use timer to interpolate camera distance smoothly
			UWorld* World = GetWorld();
			if (IsValid(World))
			{
				// Clear any existing camera interpolation timer
				World->GetTimerManager().ClearTimer(CameraInterpTimerHandle);
				
				// Set target immediately if very close, otherwise interpolate
				if (FMath::IsNearlyEqual(CurrentDistance, TargetDistance, 1.0f))
				{
					CameraBoom->TargetArmLength = TargetDistance;
				}
				else
				{
					// Interpolate camera distance over time
					FTimerDelegate InterpDelegate;
					InterpDelegate.BindLambda([this, CameraBoom, TargetDistance, CurrentDistance]()
					{
						if (!IsValid(CameraBoom)) return;
						
						float NewDistance = FMath::FInterpTo(
							CameraBoom->TargetArmLength,
							TargetDistance,
							GetWorld()->GetDeltaSeconds(),
							CameraInterpSpeed
						);
						
						CameraBoom->TargetArmLength = NewDistance;
						
						// Stop interpolation when close enough
						if (FMath::IsNearlyEqual(NewDistance, TargetDistance, 1.0f))
						{
							CameraBoom->TargetArmLength = TargetDistance;
							if (UWorld* World = GetWorld())
							{
								World->GetTimerManager().ClearTimer(CameraInterpTimerHandle);
							}
						}
					});
					
					World->GetTimerManager().SetTimer(
						CameraInterpTimerHandle,
						InterpDelegate,
						0.016f, // ~60fps update rate
						true // Loop
					);
				}
			}
		}
	}
}
*/
// Tag-change callback: cancel sprint the instant IsInAir is granted (NewCount > 0).
// Fires reactively from ASC tag event — no polling delay.
// Same delegate-driven pattern as GA_OSBlock's OnEffectAppliedToSelf for guard break.
void UGA_OSSprint::OnInAirTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}