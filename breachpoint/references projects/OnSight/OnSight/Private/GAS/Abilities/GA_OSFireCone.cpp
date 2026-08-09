// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/GA_OSFireCone.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/OSCharacter.h"
#include "Data/OSGameplayTags.h"
#include "GAS/Effects/GE_OSApplyDamage.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "Utilities/AbilityHelper.h"
#include "GAS/Components/OSAbilitySystemComponent.h"

UGA_OSFireCone::UGA_OSFireCone()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	//DamageEffectClass = UGE_OSApplyDamage::StaticClass();

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	// Ability tag — used for activation by tag and identification
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Magic_FireCone);
	SetAssetTags(AssetTags);

	// State tag applied while the ability is active
	ActivationOwnedTags.AddTag(Tags.IsAttacking);

	// Block these while casting
	BlockAbilitiesWithTag.AddTag(Tags.Attack);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(Tags.Ability_Dodge);

	// Can't cast while in these states
	ActivationBlockedTags.AddTag(Tags.IsDead);
	ActivationBlockedTags.AddTag(Tags.IsStunned);
	ActivationBlockedTags.AddTag(Tags.IsHitReacting);
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);

	// Default exclude tags: don't hit dead or invincible targets
	ExcludeTargetTags.AddTag(Tags.IsDead);
	ExcludeTargetTags.AddTag(Tags.State_Invincibility);
}

void UGA_OSFireCone::ActivateAbility(
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

	HitActorsThisCast.Reset();

	// Safety net: if FireMontage is missing, we never receive the anim-notify gameplay
	// event that would call OnDamageEventReceived -> OSEndAbility. Without this, IsAttacking
	// can stay active indefinitely (#63-style stuck attack state).
	if (!FireMontage)
	{
		if (HasAuthority(&CurrentActivationInfo))
		{
			PerformConeDamage();
		}
		OSEndAbility();
		return;
	}

	// Play the fire cast montage
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, FireMontage, 1.0f);
	if (!IsValid(MontageTask))
	{
		// Should never happen in normal montage usage, but avoid "hang forever".
		if (HasAuthority(&CurrentActivationInfo))
		{
			PerformConeDamage();
		}
		OSEndAbility();
		return;
	}
	MontageTask->OnCompleted.AddDynamic(this, &UGA_OSFireCone::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_OSFireCone::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_OSFireCone::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_OSFireCone::OnMontageFinished);
	MontageTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, FOSGameplayTags::Get().Event_DirectDamage);

	EventTask->EventReceived.AddDynamic(this, &UGA_OSFireCone::OnDamageEventReceived);
	EventTask->ReadyForActivation();
}

void UGA_OSFireCone::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Remove persistent fire cue actor
	//if (bFireCueActive && FireFXTag.IsValid() && ASC())
	//{
	//	ASC()->RemoveGameplayCue(FireFXTag);
	//	bFireCueActive = false;
	//}

	HitActorsThisCast.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_OSFireCone::OnDamageEventReceived(FGameplayEventData Payload)
{
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	// Perform the initial cone damage
	PerformConeDamage();

	// Add persistent gameplay cue actor (on all machines)
	//RunCue(FireFXTag);
	//bFireCueActive = true;
}

void UGA_OSFireCone::OnMontageFinished()
{
	
	OSEndAbility();
}

void UGA_OSFireCone::PerformConeDamage()
{
	AOSCharacter* Instigator = GetOwningCharacter();
	if (!Instigator)
	{
		return;
	}

	TArray<AOSCharacter*> Targets = GatherTargetsInCone();

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugCone)
	{
		DrawDebugConeVisualization(Instigator->GetActorLocation(), Instigator->GetActorForwardVector(), Targets);
	}
#endif

	
	for (AOSCharacter* Victim : Targets)
	{
		if (!Victim || HitActorsThisCast.Contains(Victim))
		{
			continue;
		}
		
		auto Context = AbilityHelper::BuildEffectContext(Instigator, Victim, AttackType);
		ApplyGameplayEffects(Victim, Context);
		HitActorsThisCast.Add(Victim);
	}
}

TArray<AOSCharacter*> UGA_OSFireCone::GatherTargetsInCone() const
{
	TArray<AOSCharacter*> Results;

	const AOSCharacter* Char = GetOwningCharacter();
	if (!Char)
	{
		return Results;
	}

	const FVector Origin = Char->GetActorLocation();
	const FVector Forward = Char->GetActorForwardVector();

	// Sphere overlap to get candidates
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Char);

	Char->GetWorld()->OverlapMultiByChannel(
		Overlaps,
		Origin,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(ConeRange),
		QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AOSCharacter* HitCharacter = Cast<AOSCharacter>(Overlap.GetActor());
		if (!HitCharacter || HitCharacter == Char)
		{
			continue;
		}

		// Check cone angle
		if (!IsInCone(Origin, Forward, HitCharacter))
		{
			continue;
		}

		// Check exclude tags
		// Alex: Me likey. I will yoink this and see if I can make it the same for all abilities.
		UAbilitySystemComponent* TargetASC = HitCharacter->GetAbilitySystemComponent();
		if (TargetASC && ExcludeTargetTags.Num() > 0 && TargetASC->HasAnyMatchingGameplayTags(ExcludeTargetTags))
		{
			continue;
		}
		Results.Add(HitCharacter);
	}

	return Results;
}

bool UGA_OSFireCone::IsInCone(const FVector& Origin, const FVector& Forward, const AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	const FVector ToTarget = (Target->GetActorLocation() - Origin).GetSafeNormal();
	const float DotProduct = FVector::DotProduct(Forward, ToTarget);
	const float ConeThreshold = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDegrees));

	return DotProduct >= ConeThreshold;
}

#if ENABLE_DRAW_DEBUG
void UGA_OSFireCone::DrawDebugConeVisualization(const FVector& Origin, const FVector& Forward, const TArray<AOSCharacter*>& HitTargets) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float HalfAngleRad = FMath::DegreesToRadians(ConeHalfAngleDegrees);

	// Draw the cone wireframe using line segments
	const FVector Direction = Forward.GetSafeNormal();
	const FVector EndCenter = Origin + Direction * ConeRange;
	const float EndRadius = FMath::Tan(HalfAngleRad) * ConeRange;

	// Build orthonormal basis for the cone cross-section
	FVector Right, Up;
	Direction.FindBestAxisVectors(Up, Right);

	constexpr int32 NumSegments = 24;
	const float AngleStep = 2.0f * PI / NumSegments;

	// Draw the cone outline
	FVector PrevPoint = EndCenter + Right * EndRadius;
	for (int32 i = 1; i <= NumSegments; i++)
	{
		const float Angle = AngleStep * i;
		const FVector Point = EndCenter + (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * EndRadius;

		// Ring at the end of the cone
		DrawDebugLine(World, PrevPoint, Point, FColor::Orange, false, DebugDrawDuration, 0, 1.5f);

		// Spoke lines from origin to ring
		if (i % (NumSegments / 8) == 0)
		{
			DrawDebugLine(World, Origin, Point, FColor::Orange, false, DebugDrawDuration, 0, 1.0f);
		}

		PrevPoint = Point;
	}

	// Draw center line
	DrawDebugLine(World, Origin, EndCenter, FColor::Yellow, false, DebugDrawDuration, 0, 2.0f);

	// Draw range sphere
	DrawDebugSphere(World, Origin, ConeRange, 16, FColor(255, 165, 0, 60), false, DebugDrawDuration, 0, 0.5f);

	// Draw origin as blue, hit targets as green
	DrawDebugSphere(World, Origin, 15.0f, 8, FColor::Blue, false, DebugDrawDuration, 0, 2.0f);

	for (const AOSCharacter* Target : HitTargets)
	{
		if (Target)
		{
			const FVector TargetLoc = Target->GetActorLocation();
			DrawDebugSphere(World, TargetLoc, 30.0f, 8, FColor::Green, false, DebugDrawDuration, 0, 2.0f);
			DrawDebugLine(World, Origin, TargetLoc, FColor::Green, false, DebugDrawDuration, 0, 1.0f);
		}
	}
}
#endif
