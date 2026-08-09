// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Abilities/GA_OSDodge.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/OSCharacter.h"
#include "GameFramework/Character.h"
#include "Utilities/OSControlState.h"
#include "Utilities/ChooserHelper.h"
#include "Data/OSGameplayTags.h"
#include "GAS/Abilities/GA_OSmantleability.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "OSLogCategories.h"

UGA_OSDodge::UGA_OSDodge()
{
	// Set ability tag for identification and cancellation
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Dodge);
	SetAssetTags(AssetTags);

	// GAS auto-manages: granted on ActivateAbility, removed on EndAbility.
	ActivationOwnedTags.AddTag(Tags.IsDodging);

	ActivationBlockedTags.AddTag(Tags.IsGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsBlocking);

	// GAS CancelAbilitiesWithTag — same attack family as UGA_OSBlock. BP CDO overrides may clear this; keep in sync.
	if (Tags.Attack.IsValid()) { CancelAbilitiesWithTag.AddTag(Tags.Attack); }
	if (Tags.Ability_ComboAttack.IsValid()) { CancelAbilitiesWithTag.AddTag(Tags.Ability_ComboAttack); }
	if (Tags.Ability_ComboAttackHeavy.IsValid()) { CancelAbilitiesWithTag.AddTag(Tags.Ability_ComboAttackHeavy); }
	if (Tags.Ability_ChargedAttack.IsValid()) { CancelAbilitiesWithTag.AddTag(Tags.Ability_ChargedAttack); }
	if (Tags.Ability_Sprint.IsValid()) { CancelAbilitiesWithTag.AddTag(Tags.Ability_Sprint); }
}

bool UGA_OSDodge::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(Tags.IsAttacking) && !ASC->HasMatchingGameplayTag(Tags.CanCancel_Dodge))
		{
			return false;
		}
	}
	return true;
}

FOSDodgeInfoStruct UGA_OSDodge::EvaluateDodge()
{	
	FOSDodgeInfoStruct DodgeInfo;
	auto avatar = GetOwningCharacter();
	if (!avatar) return DodgeInfo;
	
	//auto vel = avatar->GetVelocity();
	FVector InputVec = avatar->GetPendingMovementInputVector();
	DodgeInfo.bHasMoved = !InputVec.IsNearlyZero();
	DodgeInfo.bHasTarget = false;
	DodgeInfo.MoveDirection = { InputVec.X, InputVec.Y };
	
	return DodgeInfo;
}

void UGA_OSDodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// If a mantle is active, cancel it first but avoid snapping back to pre-mantle.
	// (We do this BEFORE Super::ActivateAbility because Super commits and triggers built-in cancellation.)
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		if (Tags.Ability_Mantle.IsValid())
		{
			FGameplayTagContainer MantleTags;
			MantleTags.AddTag(Tags.Ability_Mantle);

			// GetActiveAbilitiesWithTags was removed in UE 5.6.
			// Find the active mantle instance directly by cast (InstancedPerActor = one instance).
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (UGA_OSmantleability* MantleGA = Cast<UGA_OSmantleability>(Spec.GetPrimaryInstance()))
				{
					if (MantleGA->IsActive())
					{
						MantleGA->SetSkipRollbackOnCancel(true);
					}
					break; // InstancedPerActor — only one mantle spec exists
				}
			}

			ASC->CancelAbilities(&MantleTags, nullptr, this);
		}
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return; // Super may EndAbility on commit failure

	auto avatar = Cast<ACharacter>(GetOwningCharacter());
	if (!avatar || !ChooserTable)
	{
		UE_LOG(LogOSCombat, Warning, TEXT("GA_OSDodge: failed — %s"),
			!avatar ? TEXT("no avatar") : TEXT("no ChooserTable"));
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	
	UOSControlState::SetTraceCollision(avatar,ECC_Pawn, ECR_Ignore);
	FOSDodgeInfoStruct DodgeInfo = EvaluateDodge();
	
	FVector DashDir;
	if (DodgeInfo.bHasMoved)
	{
		DashDir = FVector(DodgeInfo.MoveDirection.X, DodgeInfo.MoveDirection.Y, 0.f).GetSafeNormal();
	}
	else
	{
		DashDir = avatar->GetActorForwardVector();
		DashDir.Z = 0.f;
		DashDir.Normalize();
	}
	
	UCharacterMovementComponent* CMC = avatar->GetCharacterMovement();
	if (!CMC)
	{
		UE_LOG(LogOSCombat, Warning, TEXT("GA_OSDodge: failed — no CharacterMovementComponent"));
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	CachedFallingLateralFriction = CMC->FallingLateralFriction;
	CachedBrakingDeceleration = CMC->BrakingDecelerationWalking;
	CMC->FallingLateralFriction = AirDashLateralFriction;
	CMC->AddImpulse(DashDir * DashImpulseStrength, true);
	
	FVector Vel = CMC->Velocity;
	float MaxDashSpeed = DashImpulseStrength;
	if (Vel.SizeSquared2D()> FMath::Square(MaxDashSpeed))
	{
		Vel = Vel.GetSafeNormal2D() * MaxDashSpeed;
		Vel.Z = CMC->Velocity.Z;
		CMC->Velocity = Vel;
	}
	
	auto montage = OSChooser::Evaluate<UAnimMontage>(ChooserTable, DodgeInfo);
	if (!montage)
	{
		UE_LOG(LogOSCombat, Warning, TEXT("GA_OSDodge: failed — Chooser returned null montage"));
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	PrepTaskFromMontage(montage);

	/* Publish Event.Dodge.Started after impulse is committed. Reaction abilities
	   (FlameDash etc.) subscribe via AbilityTrigger; dodge itself stays element-agnostic. */
	FGameplayEventData Payload;
	Payload.Instigator = avatar;
	Payload.Target = avatar;
	Payload.EventTag = FOSGameplayTags::Get().Event_Dodge_Started;
	Payload.EventMagnitude = 0.f;
	Payload.ContextHandle.AddInstigator(avatar, avatar);
	Payload.ContextHandle.AddOrigin(avatar->GetActorLocation());
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(avatar, Payload.EventTag, Payload);
	UE_LOG(LogOSCombat, Verbose, TEXT("GA_OSDodge: Event.Dodge.Started sent at %s"), *avatar->GetActorLocation().ToString());
}

void UGA_OSDodge::ConnectTask(UAbilityTask* task)
{
	Super::ConnectTask(task);
	if (!task)
	{
		UE_LOG(LogOSCombat, Warning, TEXT("GA_OSDodge: ConnectTask received null task"));
		return;
	}
	if (auto* t = Cast<UAbilityTask_PlayMontageAndWait>(task))
	{
		t->OnCompleted.AddDynamic(this, &UGA_OSDodge::OSEndAbility);
		t->OnBlendOut.AddDynamic(this, &UGA_OSDodge::OSEndAbility);
		t->OnInterrupted.AddDynamic(this, &UGA_OSDodge::OSEndAbility);
		t->OnCancelled.AddDynamic(this, &UGA_OSDodge::OSEndAbility);
	}
	else
	{
		UE_LOG(LogOSCombat, Warning, TEXT("GA_OSDodge: ConnectTask received unexpected task type: %s"),
			*GetNameSafe(task->GetClass()));
	}
}

// IsDodging tag is auto-removed by GAS (ActivationOwnedTags lifecycle). Restore pawn collision.
void UGA_OSDodge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (auto avatar = Cast<ACharacter>(GetOwningCharacter()))
	{
		UOSControlState::SetTraceCollision(avatar, ECC_Pawn, ECR_Block);

		if (UCharacterMovementComponent* CMC = avatar->GetCharacterMovement())
		{
			CMC->FallingLateralFriction = CachedFallingLateralFriction;
			CMC->BrakingDecelerationWalking = PostDodgeBrakingDeceleration;
		}

		// Publish Event.Dodge.Ended with the end location so reaction abilities can finalise their trail.
		FGameplayEventData Payload;
		Payload.Instigator = avatar;
		Payload.Target = avatar;
		Payload.EventTag = FOSGameplayTags::Get().Event_Dodge_Ended;
		Payload.EventMagnitude = bWasCancelled ? 1.f : 0.f;
		Payload.ContextHandle.AddInstigator(avatar, avatar);
		Payload.ContextHandle.AddOrigin(avatar->GetActorLocation());
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(avatar, Payload.EventTag, Payload);
		UE_LOG(LogOSCombat, Verbose, TEXT("GA_OSDodge: Event.Dodge.Ended sent at %s (cancelled=%d)"),
			*avatar->GetActorLocation().ToString(), bWasCancelled ? 1 : 0);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}