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
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(BRGameplayTags::State_Dead);

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
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBRGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	// OnAvatarSet, not OnGiveAbility. The ASC lives on the PlayerState and outlives the pawn, so
	// a respawn grants nothing new - it points the existing specs at a new avatar. OnGiveAbility
	// fires once per grant and would never run again, leaving an OnSpawn ability dead for every
	// life after the first. OnAvatarSet fires on the grant AND on each new avatar.
	if (ActivationPolicy != EBRAbilityActivationPolicy::OnSpawn || !ActorInfo)
	{
		return;
	}

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

	// One line in the base rather than seven in the leaves: every BR ability calls
	// Super::ActivateAbility, so this covers the abilities that exist and the ones that do not
	// yet. It prints BEFORE the commit below, so a cost or cooldown refusal still shows the
	// activation that was attempted rather than looking like a dead input.
	//
	// ActivationMode, not just a server/client flag, because it is the one field that says
	// whether prediction is working: a client should print Predicting and then the server's own
	// Authority line. Predicting with no Authority line means the server refused.
	const TCHAR* ActivationMode =
		ActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Authority ? TEXT("Authority") :
		ActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Predicting ? TEXT("Predicting") :
		TEXT("Confirmed");

	UE_LOG(LogBRAbility, Log, TEXT("GA ACTIVATED: %s on '%s' [%s]"),
		*GetClass()->GetName(),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr),
		ActivationMode);

	if (bCommitOnActivate && !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActivationPolicy == EBRAbilityActivationPolicy::WhileInputHeld)
	{
		UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
		if (!WaitRelease)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		WaitRelease->OnRelease.AddDynamic(this, &UBRGameplayAbility::HandleInputReleased);
		WaitRelease->ReadyForActivation();
	}
}

void UBRGameplayAbility::HandleInputReleased(float TimeHeld)
{
	ExternalEndAbility();
}

const FGameplayTagContainer* UBRGameplayAbility::GetCooldownTags() const
{
	// nullptr, not the parent's tags: GE_Cooldown's CDO grants State.Cooldown, which is present
	// whenever ANY ability is cooling down. Returning it here would make an ability with no
	// cooldown of its own report itself blocked because something else is on cooldown.
	if (!CooldownTag.IsValid())
	{
		return nullptr;
	}

	// This ability's own tag ONLY, for the same reason. The parent container is deliberately not
	// appended.
	CooldownTagsScratch.Reset();
	CooldownTagsScratch.AddTag(CooldownTag);

	return &CooldownTagsScratch;
}

void UBRGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!CooldownTag.IsValid())
	{
		return;
	}

	const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE)
	{
		return;
	}

	const float DurationSeconds = GetCooldownDurationSeconds();
	if (DurationSeconds <= 0.f)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CooldownGE->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownTag);
	SpecHandle.Data->SetSetByCallerMagnitude(BRGameplayTags::SetByCaller_CooldownDuration, DurationSeconds);

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
