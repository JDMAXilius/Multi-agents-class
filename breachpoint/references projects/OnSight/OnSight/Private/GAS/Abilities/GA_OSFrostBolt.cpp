// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/GA_OSFrostBolt.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/OSCharacter.h"
#include "Data/OSGameplayTags.h"
#include "Weapons/Projectiles/OSProjectile.h"

UGA_OSFrostBolt::UGA_OSFrostBolt()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	// Ability tag
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Magic_FrostBolt);
	SetAssetTags(AssetTags);

	// State tag applied while casting
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
}

void UGA_OSFrostBolt::ActivateAbility(
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

	// CastMontage is required — without it the anim notify never fires, the projectile
	// never spawns, and the ability hangs forever (IsAttacking permanently active).
	if (!CastMontage)
	{
		UE_LOG(LogTemp, Error, TEXT("[FrostBolt] CastMontage is null — ability cannot function. Ending."));
		OSEndAbility();
		return;
	}

	// Play the cast montage
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, CastMontage, 1.0f);

	if (!IsValid(MontageTask))
	{
		OSEndAbility();
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UGA_OSFrostBolt::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_OSFrostBolt::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_OSFrostBolt::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_OSFrostBolt::OnMontageFinished);
	MontageTask->ReadyForActivation();

	// Wait for anim notify to spawn the projectile
	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, FOSGameplayTags::Get().Event_DirectDamage);

	if (IsValid(EventTask))
	{
		EventTask->EventReceived.AddDynamic(this, &UGA_OSFrostBolt::OnSpawnEventReceived);
		EventTask->ReadyForActivation();
	}
}

void UGA_OSFrostBolt::OnSpawnEventReceived(FGameplayEventData Payload)
{
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	AOSCharacter* Char = GetOwningCharacter();
	if (!Char || !ProjectileClass)
	{
		return;
	}

	const FVector Forward = Char->GetActorForwardVector();
	const FVector SpawnLocation = Char->GetActorLocation() + Forward * SpawnForwardOffset;
	const FRotator SpawnRotation = Forward.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Char;
	SpawnParams.Instigator = Char;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AOSProjectile* Projectile = GetWorld()->SpawnActor<AOSProjectile>(
		ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);

	if (Projectile)
	{
		// Only set source actor — the projectile BP owns everything else
		FOSProjectileInit InitData;
		InitData.Dir = Forward;
		InitData.SourceActor = Char;
		InitData.SourceASC = GetAbilitySystemComponentFromActorInfo();
		Projectile->InitializeProjectile(InitData);
		Projectile->ApplyInit();
	}
}

void UGA_OSFrostBolt::OnMontageFinished()
{
	OSEndAbility();
}

void UGA_OSFrostBolt::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
