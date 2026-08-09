// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Abilities/Magic/GA_OSFlameDash.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Characters/OSCharacter.h"
#include "Data/OSGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "OSLogCategories.h"

// --- Debug CVar ---

/* Runtime toggle for the dash trail debug line. Persistent 3s line from start -> end on every
   completed dash. Cheap, single-shot - safe to leave on during playtests. */
static TAutoConsoleVariable<int32> CVarFlameDashTrail(
	TEXT("OS.Debug.FlameDashTrail"), 0,
	TEXT("Draw debug line from dash start to end when FlameDash fires. 0=off, 1=on."),
	ECVF_Default);

// --- Ctor ---

UGA_OSFlameDash::UGA_OSFlameDash()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Magic_FlameDash);
	SetAssetTags(AssetTags);

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// Triggered by the dodge start event - auto-activates alongside GA_OSDodge.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = Tags.Event_Dodge_Started;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

// --- Lifecycle ---

void UGA_OSFlameDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		UE_LOG(LogOSCombat, Warning, TEXT("GA_OSFlameDash: no avatar - aborting."));
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	CachedStartLocation = Avatar->GetActorLocation();
	UE_LOG(LogOSCombat, Verbose, TEXT("GA_OSFlameDash: Triggered. Start=%s"), *CachedStartLocation.ToString());

	// Fire the trail cue immediately so the ribbon follows the player for the full dash.
	// End location is not yet known: BPGC cues attached to the target ignore it anyway.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FVector DashDir = Avatar->GetVelocity().GetSafeNormal2D();
		FGameplayCueParameters Params;
		Params.Location = CachedStartLocation;
		Params.Normal = DashDir.IsNearlyZero() ? Avatar->GetActorForwardVector() : DashDir;
		Params.RawMagnitude = 0.f; // unknown at start; stretch-beam designs should use end-fired cues
		Params.Instigator = Avatar;
		Params.SourceObject = Avatar;
		Params.OriginalTag = FOSGameplayTags::Get().Cue_Magic_Fire_FlameTrail;
		ASC->ExecuteGameplayCue(FOSGameplayTags::Get().Cue_Magic_Fire_FlameTrail, Params);
		UE_LOG(LogOSCombat, Verbose, TEXT("GA_OSFlameDash: ExecuteGameplayCue GameplayCue.Magic.Fire.FlameTrail (on start)"));
	}

	// Wait for the dodge end event. Owner filters so we only react to our own avatar's dodge.
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	WaitDodgeEndedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, Tags.Event_Dodge_Ended, /*OptionalExternalTarget*/ nullptr, /*OnlyTriggerOnce*/ true, /*OnlyMatchExact*/ true);
	if (WaitDodgeEndedTask)
	{
		WaitDodgeEndedTask->EventReceived.AddDynamic(this, &UGA_OSFlameDash::OnDodgeEnded);
		WaitDodgeEndedTask->ReadyForActivation();
	}

	// Safety cap: if Event.Dodge.Ended never fires we still clean up.
	MaxLifetimeTask = UAbilityTask_WaitDelay::WaitDelay(this, MaxLifetime);
	if (MaxLifetimeTask)
	{
		MaxLifetimeTask->OnFinish.AddDynamic(this, &UGA_OSFlameDash::OnMaxLifetimeElapsed);
		MaxLifetimeTask->ReadyForActivation();
	}
}

void UGA_OSFlameDash::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (WaitDodgeEndedTask)  { WaitDodgeEndedTask->EndTask(); WaitDodgeEndedTask = nullptr; }
	if (MaxLifetimeTask)     { MaxLifetimeTask->EndTask();    MaxLifetimeTask = nullptr; }

	UE_LOG(LogOSCombat, Verbose, TEXT("GA_OSFlameDash: EndAbility (cancelled=%d)"), bWasCancelled ? 1 : 0);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// --- Events ---

void UGA_OSFlameDash::OnDodgeEnded(FGameplayEventData Payload)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const FVector EndLocation = Avatar ? Avatar->GetActorLocation() : CachedStartLocation;

	UE_LOG(LogOSCombat, Verbose, TEXT("GA_OSFlameDash: Event.Dodge.Ended received. End=%s"), *EndLocation.ToString());

	SpawnTrail(CachedStartLocation, EndLocation);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}

void UGA_OSFlameDash::OnMaxLifetimeElapsed()
{
	UE_LOG(LogOSCombat, Warning, TEXT("GA_OSFlameDash: MaxLifetime elapsed without Event.Dodge.Ended - cleaning up."));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
}

// --- Trail ---

void UGA_OSFlameDash::SpawnTrail(const FVector& StartLocation, const FVector& EndLocation)
{
	const float Distance = FVector::Dist(StartLocation, EndLocation);
	UE_LOG(LogOSCombat, Log, TEXT("GA_OSFlameDash: SpawnTrail Start=%s End=%s Dist=%.1f"),
		*StartLocation.ToString(), *EndLocation.ToString(), Distance);

	// TODO: spawn AOSFlameTrail here and let it drive burn damage.

	if (bDebugAbility || CVarFlameDashTrail.GetValueOnGameThread() != 0)
	{
		if (const UWorld* World = GetWorld())
		{
			DrawDebugLine(World, StartLocation, EndLocation, FColor::Red, /*bPersistent*/ false, /*LifeTime*/ 3.0f, 0, /*Thickness*/ 4.0f);
			DrawDebugSphere(World, StartLocation, 20.f, 12, FColor::Yellow, false, 3.0f);
			DrawDebugSphere(World, EndLocation, 20.f, 12, FColor::Orange, false, 3.0f);
		}
	}
}
