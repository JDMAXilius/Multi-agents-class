// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/GA_OSJump.h"

#include "Characters/OSCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Data/OSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GAS/Effects/GE_OSJumpingMovement.h"

UGA_OSJump::UGA_OSJump()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	// Asset tag (TryActivateAbilitiesByTag, CancelAbilitiesWithTag from other systems)
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Jump);
	SetAssetTags(AssetTags);

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	// Instant GA — no ActivationOwnedTags (airborne / in-jump is IsInAir + movement, not this spec lifetime).

	// Cannot jump while incapacitated or in exclusive locomotion / combat setup
	// IsDead already on UOSGameplayAbility.
	ActivationBlockedTags.AddTag(Tags.IsStunned);
	ActivationBlockedTags.AddTag(Tags.IsBlocking);
	ActivationBlockedTags.AddTag(Tags.IsHitReacting);
	ActivationBlockedTags.AddTag(Tags.IsRecoiling);
	ActivationBlockedTags.AddTag(Tags.IsKnockedDown);
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsBeingGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsMantling);
	ActivationBlockedTags.AddTag(Tags.IsGrabbing);
	ActivationBlockedTags.AddTag(Tags.IsCharging);
	ActivationBlockedTags.AddTag(Tags.State_IsStunLock);
	// Jump-cancel: strip active combat when jump wins (same parent Attack tag pattern as GA_OSRecoil)
	CancelAbilitiesWithTag.AddTag(Tags.Attack);
	CancelAbilitiesWithTag.AddTag(Tags.Ability_ChargedAttack);
	CancelAbilitiesWithTag.AddTag(Tags.Ability_Grab);

	// Not BlockAbilitiesWithTag(Tags.Ability): recoil holds for a montage window; jump ends same frame so a full
	// ability lockout would barely apply — gate other actions while airborne via their ActivationBlockedTags + IsInAir.
	BlockAbilitiesWithTag.Reset();
}

// TODO: Move post event to cues.
void UGA_OSJump::ActivateAbility( const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData )
{
	UWorld* World = GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	AOSCharacter* OSCharacter = GetOwningCharacter();
	if (!OSCharacter)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UCapsuleComponent* Capsule = OSCharacter->GetCapsuleComponent();
	if (!Capsule)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector CurrentLocation = Capsule->GetComponentLocation();

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(JumpTakeoffSurface), /*bTraceComplex*/ false, OSCharacter);
	FHitResult HitResult;

	// WorldStatic: typical walkable ground. Movable platforms may need a custom channel or second trace.
	const bool bHit = World->LineTraceSingleByChannel(HitResult,
		CurrentLocation, CurrentLocation + LandTraceOffset,
		ECC_WorldStatic, TraceParams);
	
	if (bHit && HitResult.PhysMaterial.IsValid())
		BPEvent_SetWWiseSwitchBasedOnSurface(OSCharacter, HitResult.PhysMaterial->SurfaceType);
	
		
	BPEvent_PostWwiseEvent(OSCharacter, OSCharacter->IsLocallyControlled());

	OSCharacter->Jump();

	if (OSCharacter->HasAuthority())
	{
		if (UAbilitySystemComponent* ASC = OSCharacter->GetAbilitySystemComponent())
		{
			ASC->ApplyGameplayEffectToSelf(
				UGE_OSJumpingMovement::StaticClass()->GetDefaultObject<UGE_OSJumpingMovement>(),
				1.f,
				ASC->MakeEffectContext());
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}

bool UGA_OSJump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AOSCharacter* OSCharacter = GetOwningCharacter();
	if (!OSCharacter || !OSCharacter->CanJump())
	{
		return false;
	}

	return true;
}
