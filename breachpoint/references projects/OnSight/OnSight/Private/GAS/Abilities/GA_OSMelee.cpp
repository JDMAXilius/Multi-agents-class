#include "GAS/Abilities/GA_OSMelee.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToActorForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/OSCharacter.h"
#include "Data/OSCombatTypes.h"
#include "Data/OSGameplayTags.h"
#include "GAS/Effects/GE_OSApplyDamage.h"
#include "GAS/Tasks/AT_OSTickAbilityTask.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

UGA_OSMelee::UGA_OSMelee()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	if (Tags.Ability_Melee.IsValid())
	{
		AssetTags.AddTag(Tags.Ability_Melee);
		SetAssetTags(AssetTags);
	}

	const FGameplayTag AnimUseWeapon = FGameplayTag::RequestGameplayTag(TEXT("Character.Anim.UseWeapon"), false);
	const FGameplayTag StateMelee = FGameplayTag::RequestGameplayTag(TEXT("Character.State.Melee"), false);
	if (AnimUseWeapon.IsValid())
	{
		ActivationOwnedTags.AddTag(AnimUseWeapon);
	}
	if (StateMelee.IsValid())
	{
		ActivationOwnedTags.AddTag(StateMelee);
	}

	// UOSGameplayAbility: IsDead. Shared combat lockout with UGA_OSAttack (this GA is not under GA_OSAttack).
	ActivationBlockedTags.AddTag(Tags.IsStunned);
	ActivationBlockedTags.AddTag(Tags.IsHitReacting);
	ActivationBlockedTags.AddTag(Tags.IsRecoiling);
	ActivationBlockedTags.AddTag(Tags.IsKnockedDown);
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsBeingGrabbed);
	ActivationBlockedTags.AddTag(Tags.IsMantling);
	ActivationBlockedTags.AddTag(Tags.State_IsStunLock);

	const FGameplayTag HasInput = FGameplayTag::RequestGameplayTag(TEXT("Ability.Hasinput"), false);
	if (HasInput.IsValid())
	{
		CancelAbilitiesWithTag.AddTag(HasInput);
		BlockAbilitiesWithTag.AddTag(HasInput);
	}

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	WeaponMetrics.SetNum((int32)EOSWeaponMetrics::MAX);
	WeaponMetrics[(int32)EOSWeaponMetrics::Rank] = 20.f;
	WeaponMetrics[(int32)EOSWeaponMetrics::SwingThreashold] = 1.f;
	WeaponMetrics[(int32)EOSWeaponMetrics::SwingTreshold] = 1.f;
	WeaponMetrics[(int32)EOSWeaponMetrics::MeleeRange] = 150.f;
	WeaponMetrics[(int32)EOSWeaponMetrics::WeakPoint] = 1.f;

	DamageEffectClass = UGE_OSApplyDamage::StaticClass();

	HitWindowStartTag = FGameplayTag::RequestGameplayTag(TEXT("Character.Anim.MeleeHitWindow.Start"), false);
	HitWindowEndTag = FGameplayTag::RequestGameplayTag(TEXT("Character.Anim.MeleeHitWindow.End"), false);
}

float UGA_OSMelee::GetWeaponMetric(EOSWeaponMetrics Index) const
{
	const int32 I = static_cast<int32>(Index);
	if (WeaponMetrics.IsValidIndex(I))
	{
		return WeaponMetrics[I];
	}
	return 0.f;
}

void UGA_OSMelee::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	AOSCharacter* Character = Avatar();
	if (!IsValid(Character) || !MontageToPlay)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	HitActorsThisSwing.Reset();
	ValidTarget = nullptr;
	ActiveSweepTask = nullptr;
	ActiveRootMotionTask = nullptr;

	ValidTarget = MeleeTarget.Get();
	if (!ValidTarget.IsValid())
	{
		ValidTarget = Character->SoftLockTarget;
	}

	OnMotionWarpTarget();

	if (bUseRootMotionGapClose && ValidTarget.IsValid())
	{
		SetupMotionWarpAndMoveToTarget();
	}

	if (HitWindowStartTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* WaitStart = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, HitWindowStartTag, nullptr, true, true);
		WaitStart->EventReceived.AddDynamic(this, &UGA_OSMelee::OnMeleeHitWindowStart);
		WaitStart->ReadyForActivation();
	}

	if (HitWindowEndTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* WaitEnd = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, HitWindowEndTag, nullptr, true, true);
		WaitEnd->EventReceived.AddDynamic(this, &UGA_OSMelee::OnMeleeHitWindowEnd);
		WaitEnd->ReadyForActivation();
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay, 1.f, NAME_None, false);
	MontageTask->OnCompleted.AddDynamic(this, &UGA_OSMelee::OnMeleeMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_OSMelee::OnMeleeMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_OSMelee::OnMeleeMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_OSMelee::OnMeleeMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UGA_OSMelee::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearMeleeWarp();
	ActiveSweepTask = nullptr;
	ActiveRootMotionTask = nullptr;
	HitActorsThisSwing.Reset();
	ValidTarget = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_OSMelee::OnMotionWarpTarget()
{
	AOSCharacter* Character = Avatar();
	if (!IsValid(Character))
	{
		return;
	}

	UMotionWarpingComponent* MWC = Character->GetMotionWarpingComponent();
	if (!MWC || MotionWarpTargetName.IsNone())
	{
		return;
	}

	if (ValidTarget.IsValid())
	{
		const FVector Loc = ValidTarget->GetActorLocation();
		MWC->AddOrUpdateWarpTargetFromTransform(MotionWarpTargetName, FTransform(ValidTarget->GetActorRotation(), Loc));
	}
	else
	{
		MWC->RemoveWarpTarget(MotionWarpTargetName);
	}
}

void UGA_OSMelee::SetupMotionWarpAndMoveToTarget()
{
	if (!ValidTarget.IsValid())
	{
		return;
	}

	UAbilityTask_ApplyRootMotionMoveToActorForce* Task = UAbilityTask_ApplyRootMotionMoveToActorForce::ApplyRootMotionMoveToActorForce(
		this,
		FName(TEXT("MeleeRootMotionGapClose")),
		ValidTarget.Get(),
		FVector::ZeroVector,
		ERootMotionMoveToActorTargetOffsetType::AlignFromTargetToSource,
		RootMotionDuration,
		nullptr,
		nullptr,
		false,
		MOVE_Walking,
		true,
		PathOffsetCurve,
		nullptr,
		ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity,
		FVector::ZeroVector,
		0.f,
		false,
		ReachedDestinationDistance);

	if (Task)
	{
		Task->OnFinished.AddDynamic(this, &UGA_OSMelee::OnRootMotionMoveFinished);
		Task->ReadyForActivation();
		ActiveRootMotionTask = Task;
	}
}

void UGA_OSMelee::ClearMeleeWarp()
{
	AOSCharacter* Character = Avatar();
	if (!IsValid(Character) || MotionWarpTargetName.IsNone())
	{
		return;
	}
	if (UMotionWarpingComponent* MWC = Character->GetMotionWarpingComponent())
	{
		MWC->RemoveWarpTarget(MotionWarpTargetName);
	}
}

void UGA_OSMelee::OnRootMotionMoveFinished(bool bDestinationReached, bool bTimedOut, FVector FinalTargetLocation)
{
	ClearMeleeWarp();
	ActiveRootMotionTask = nullptr;
}

void UGA_OSMelee::OnMeleeHitWindowStart(FGameplayEventData Payload)
{
	if (ActiveSweepTask)
	{
		return;
	}

	UAT_OSTickAbilityTask* TickTask = UAT_OSTickAbilityTask::CreateTickTask(this, FName(TEXT("MeleeSweepTick")));
	if (!TickTask)
	{
		return;
	}

	TickTask->OnTick.AddDynamic(this, &UGA_OSMelee::OnMeleeSweepTick);
	TickTask->ReadyForActivation();
	ActiveSweepTask = TickTask;
}

void UGA_OSMelee::OnMeleeHitWindowEnd(FGameplayEventData Payload)
{
	if (ActiveSweepTask)
	{
		ActiveSweepTask->EndTask();
		ActiveSweepTask = nullptr;
	}
}

void UGA_OSMelee::OnMeleeSweepTick(float DeltaTime)
{
	AOSCharacter* Character = Avatar();
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !World || !Character->HasAuthority())
	{
		return;
	}

	const float Range = FMath::Max(1.f, GetWeaponMetric(EOSWeaponMetrics::MeleeRange));
	const float Radius = FMath::Max(1.f, SweepSphereRadius);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeSweep), false, Character);
	Params.AddIgnoredActor(Character);

	const FVector Forward = Character->GetActorForwardVector();
	const FVector Start = Character->GetActorLocation() + FVector(0.f, 0.f, SweepTraceZOffset);
	const FVector End = Start + Forward * Range;

	FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
	TArray<FHitResult> Hits;
	if (!World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, Sphere, Params))
	{
		return;
	}

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor) || HitActor == Character)
		{
			continue;
		}
		if (HitActorsThisSwing.Contains(HitActor))
		{
			continue;
		}

		AOSCharacter* Victim = Cast<AOSCharacter>(HitActor);
		if (!IsValid(Victim))
		{
			continue;
		}

		HitActorsThisSwing.Add(Victim);
		TryApplyMeleeDamage(Victim);
	}
}

void UGA_OSMelee::TryApplyMeleeDamage(AOSCharacter* Victim)
{
	AOSCharacter* Character = Avatar();
	if (!IsValid(Character) || !IsValid(Victim))
	{
		return;
	}

	const float Base = GetWeaponMetric(EOSWeaponMetrics::Rank);
	const float WeakMul = FMath::Max(0.f, GetWeaponMetric(EOSWeaponMetrics::WeakPoint));
	const float Damage = Base * (WeakMul > 0.f ? WeakMul : 1.f);

	UOSCombatBlueprintLibrary::ApplyDirectDamage(Character, Victim, DamageEffectClass, Damage, EOSAttackType::Light);
}

void UGA_OSMelee::OnMeleeMontageEnded()
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		OSEndAbility();
	}
}

void UGA_OSMelee::OnMeleeMontageInterrupted()
{
	OSCancelAbility();
}
