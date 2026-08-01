// Breachpoint. The ability base: activation policy, generic cooldown, cancel hygiene, one death gate.

#include "AbilitySystem/Abilities/BRGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"

UBRGameplayAbility::UBRGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// §3.3. InstancedPerActor because every ability in this game holds per-activation state
	// (targets, tasks, montages) and non-instanced abilities are deprecated in 5.5 anyway.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// LocalPredicted for everything a player FEELS. The client starts the ability immediately and
	// the server confirms or rejects; a rejected activation rolls back its own cost, cooldown and
	// cues because all three went through GAS (law 4). Server-originated abilities (respawn grants)
	// override this to ServerInitiated in their own constructor.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// THE ONE DEATH GATE (gas-purity.md law 8). GE_Death grants State.Dead; every ability in the
	// project inherits this line and therefore stops. There is no second check anywhere, and any
	// `if (bIsDead)` found in an ability body is a finding against this class, not a safety net.
	ActivationBlockedTags.AddTag(BRGameplayTags::State_Dead);

	// ONE cooldown effect for the whole game. The per-ability part is the tag and the duration,
	// both of which arrive on the SPEC — see ApplyCooldown.
	CooldownGameplayEffectClass = UBRGE_Cooldown::StaticClass();
}

UBRAbilitySystemComponent* UBRGameplayAbility::GetBRAbilitySystemComponent() const
{
	return CurrentActorInfo ? Cast<UBRAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr;
}

void UBRGameplayAbility::ExternalEndAbility()
{
	if (!IsActive())
	{
		// Reachable and harmless: the ability ended itself inside the same RPC batch that is now
		// trying to end it. Verbose rather than silent so a double-end is traceable if it ever
		// turns out to matter.
		UE_LOG(LogBRCombat, Verbose, TEXT("UBRGameplayAbility::ExternalEndAbility on '%s' which is not active; ignored."), *GetName());
		return;
	}

	// bReplicateEndAbility TRUE: the other side must learn the ability finished, or a batched
	// fire leaves a phantom active ability on the server.
	// bWasCancelled FALSE: this is a completion. Cancelling would fire cancel tags and tell every
	// task it was interrupted, which is a different event with different consequences.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UBRGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	if (ActivationPolicy != EBRAbilityActivationPolicy::OnSpawn || !ActorInfo)
	{
		return;
	}

	// OnSpawn abilities activate the moment they are granted, on the granting authority. Anything
	// else (activating from a client that just received the grant replication) would double-run it.
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		if (!Spec.IsActive() && ASC->GetOwner() && ASC->GetOwner()->HasAuthority())
		{
			ASC->TryActivateAbility(Spec.Handle);
		}
	}
}

void UBRGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (bCommitOnActivate && !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// Cost or cooldown said no. END, and end as CANCELLED — a committed-but-refused activation
		// that quietly continues is how an ability fires without paying for itself.
		UE_LOG(LogBRCombat, Verbose, TEXT("UBRGameplayAbility '%s': CommitAbility failed; ending."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	if (ActivationPolicy == EBRAbilityActivationPolicy::WhileInputHeld)
	{
		// bTestAlreadyReleased=true is the tap case and it is not an edge case: at 60 Hz a quick
		// tap CAN release before a LocalPredicted activation completes, and without this the
		// ability never ends — you sprint forever having pressed the key once.
		UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/true);
		if (!WaitRelease)
		{
			// Refused rather than continued: a WhileInputHeld ability with no release watcher is an
			// ability that never ends, which is worse than one that never starts.
			UE_LOG(LogBRCombat, Error, TEXT("UBRGameplayAbility '%s': WhileInputHeld could not create its WaitInputRelease task; ending immediately rather than running forever."), *GetName());
			EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
			return;
		}

		WaitRelease->OnRelease.AddDynamic(this, &UBRGameplayAbility::HandleInputReleased);
		WaitRelease->ReadyForActivation();
	}
}

void UBRGameplayAbility::HandleInputReleased(float TimeHeld)
{
	UE_LOG(LogBRCombat, Verbose, TEXT("UBRGameplayAbility '%s': input released after %.3f s; ending (WhileInputHeld)."), *GetName(), TimeHeld);

	// A completion, not a cancel: letting go of sprint is the ability finishing normally.
	ExternalEndAbility();
}

// ---------------------------------------------------------------------------
// The generic cooldown
// ---------------------------------------------------------------------------

const FGameplayTagContainer* UBRGameplayAbility::GetCooldownTags() const
{
	if (!CooldownTag.IsValid())
	{
		// No cooldown declared. Returning the base's (empty for GE_Cooldown, which grants nothing
		// of its own) keeps CheckCooldown a no-op instead of blocking on a tag nobody grants.
		return Super::GetCooldownTags();
	}

	CooldownTagsScratch.Reset();
	if (const FGameplayTagContainer* ParentTags = Super::GetCooldownTags())
	{
		CooldownTagsScratch.AppendTags(*ParentTags);
	}
	CooldownTagsScratch.AddTag(CooldownTag);

	return &CooldownTagsScratch;
}

void UBRGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!CooldownTag.IsValid())
	{
		// Declared no cooldown. Not an error and not a silent skip: this IS the answer for sprint,
		// which is free by design (GDD: "Sprint — no cost").
		return;
	}

	const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE)
	{
		UE_LOG(LogBRCombat, Error, TEXT("UBRGameplayAbility '%s' declares cooldown tag '%s' but has no cooldown effect class; NO cooldown was applied."),
			*GetName(), *CooldownTag.ToString());
		return;
	}

	const float DurationSeconds = GetCooldownDurationSeconds();
	if (DurationSeconds <= 0.f)
	{
		// REFUSED, LOUDLY. A zero-length cooldown applies successfully and blocks nothing, so the
		// ability looks like it has a cooldown and does not. Silence here is how the rocket ends up
		// firing at the trigger's rate.
		UE_LOG(LogBRCombat, Error, TEXT("UBRGameplayAbility '%s' declares cooldown tag '%s' but GetCooldownDurationSeconds() returned %.3f. NO cooldown applied — fix the data or clear the tag."),
			*GetName(), *CooldownTag.ToString(), DurationSeconds);
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CooldownGE->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogBRCombat, Error, TEXT("UBRGameplayAbility '%s': failed to build a GE_Cooldown spec; NO cooldown applied."), *GetName());
		return;
	}

	// THE GENERIC-COOLDOWN TRICK, both halves in two lines: the ability's own tag is granted by the
	// shared effect through the SPEC, and the duration is a SetByCaller. One GE class, every
	// ability, per-ability durations from data.
	SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownTag);
	SpecHandle.Data->SetSetByCallerMagnitude(BRGameplayTags::SetByCaller_CooldownDuration, DurationSeconds);

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
