// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/GA_OSHitReaction.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GAS/Effects/GE_OSHitReactState.h"
#include "GAS/Effects/GE_OSKnockDown.h"
#include "Data/OSGameplayTags.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "Utilities/ChooserHelper.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"
#include "UObject/EnumProperty.h"
#include "AbilitySystemComponent.h"
#include "OSLogCategories.h"

static FString ToString_Direction(EOSDirection Dir)
{
	if (const UEnum* Enum = StaticEnum<EOSDirection>())
	{
		return Enum->GetNameStringByValue(static_cast<int64>(Dir));
	}
	return TEXT("Invalid");
}

static FString ToString_ReactType(EOSHitReactType Type)
{
	if (const UEnum* Enum = StaticEnum<EOSHitReactType>())
	{
		return Enum->GetNameStringByValue(static_cast<int64>(Type));
	}
	return TEXT("Invalid");
}

UGA_OSHitReaction::UGA_OSHitReaction()
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_HitReaction);
	SetAssetTags(AssetTags);

	// Trigger off GameplayEvent.HitReact without requiring BP setup.
	FAbilityTriggerData Trigger;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	Trigger.TriggerTag = Tags.Event_HitReact;
	AbilityTriggers.Add(Trigger);

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	ActivationBlockedTags.AddTag(Tags.IsDead);
	ActivationBlockedTags.AddTag(Tags.IsGrabbed);

	HitReactStateEffectClass = UGE_OSHitReactState::StaticClass();
	KnockDownEffectClass = UGE_OSKnockDown::StaticClass();

	// FIX: Explicitly cancel active attacks when a hit reaction starts.
	// This prevents competing Root Motion sources from causing teleports/jitter.
	// BP combos use Attack.Light / Attack.Heavy asset tags — Attack is the parent that matches both.
	CancelAbilitiesWithTag.AddTag(Tags.Attack);
	CancelAbilitiesWithTag.AddTag(Tags.Ability_ChargedAttack);
	CancelAbilitiesWithTag.AddTag(Tags.Ability_Grab);
	CancelAbilitiesWithTag.AddTag(Tags.Ability_Recoil);

	IgnoredTagsOnCancel.AddTag(Tags.Ability_Block);
}

EOSHitReactType UGA_OSHitReaction::ReactTypeFromEventMagnitude(float Magnitude)
{
	const int32 TypeInt = FMath::RoundToInt(Magnitude);
	switch (TypeInt)
	{
	case 0: return EOSHitReactType::Light;
	case 1: return EOSHitReactType::Heavy;
	case 2: return EOSHitReactType::Knockdown;
	case 3: return EOSHitReactType::Launch;
	case 4: return EOSHitReactType::Death;
	case 6: return EOSHitReactType::GuardBreak;
	default: return EOSHitReactType::Light;
	}
}



void UGA_OSHitReaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// No costs/cooldowns usually, but keep commit discipline.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogOSGASHitReact, Verbose, TEXT("[HitReact] CommitAbility failed. Owner=%s"), *GetNameSafe(GetOwningActorFromActorInfo()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		UE_LOG(LogOSGASHitReact, Warning, TEXT("[HitReact] Missing Avatar. Avatar=%s"), *GetNameSafe(Avatar));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (*HitReactStateEffectClass)
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, HitReactStateEffectClass.GetDefaultObject(), GetAbilityLevel());
	}

	// Choose montage by (ReactType + HitDirection). Always keep a safe default fallback.
	UAnimMontage* MontageToPlay = DefaultHitReactMontage;

	EOSHitReactType ReactType = EOSHitReactType::Light;
	EOSDirection Dir = EOSDirection::FRONT;
	FHitResult HitForDir;

	if (TriggerEventData)
	{
		ReactType = ReactTypeFromEventMagnitude(TriggerEventData->EventMagnitude);

		if (const FHitResult* Hit = TriggerEventData->ContextHandle.GetHitResult())
		{
			HitForDir = *Hit;
			Dir = UOSCombatBlueprintLibrary::ComputeDirection4WayFromHit(Avatar, *Hit);
		}
	}

	// Apply knockdown GE when the react type is Knockdown (grants State.KnockedDown for a duration).
	if (ReactType == EOSHitReactType::Knockdown && *KnockDownEffectClass)
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, KnockDownEffectClass.GetDefaultObject(), GetAbilityLevel());
	}

	// Build the chooser context struct once — used for both block and normal paths.
	FOSHitReacts ChooserContext;
	ChooserContext.HitDirection = Dir;
	ChooserContext.ReactDirection = Dir;
	ChooserContext.ReactType = ReactType;
	if (auto ASC = GetAbilitySystemComponentFromActorInfo())
		ChooserContext.IsInAir = ASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsInAir);
	
	// Limb left at default HEAD unless hit-location data is added later.

	/* The ASC has to be valid and have the matching tag*/
	const bool bIsBlocking = ActorInfo->AbilitySystemComponent.IsValid() && ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(FOSGameplayTags::Get().IsBlocking);

	if (bIsBlocking && BlockHitReactChooser)
	{
		if (UAnimMontage* BlockChosen = OSChooser::Evaluate<UAnimMontage>(BlockHitReactChooser, ChooserContext))
		{
			MontageToPlay = BlockChosen;
			UE_LOG(LogOSGASHitReact, Verbose, TEXT("[HitReact] Block reaction: ReactType=%s Dir=%s Montage=%s"),
				*ToString_ReactType(ReactType), *ToString_Direction(Dir), *GetNameSafe(MontageToPlay));
		}
		else
		{
			UE_LOG(LogOSGASHitReact, Verbose, TEXT("[HitReact] BlockHitReactChooser returned nothing for ReactType=%s Dir=%s; falling through to base chooser."),
				*ToString_ReactType(ReactType), *ToString_Direction(Dir));
		}
	}

	if (MontageToPlay == DefaultHitReactMontage && HitReactChooser)
	{
		if (UAnimMontage* Chosen = OSChooser::Evaluate<UAnimMontage>(HitReactChooser, ChooserContext))
		{
			MontageToPlay = Chosen;
		}
		else
		{
			// Chooser returned nothing (e.g. Heavy row: ensure nested result is AnimMontage, not Animation Sequence)
			UE_LOG(LogOSGASHitReact, Verbose, TEXT("[HitReact] Chooser returned no montage for ReactType=%s Dir=%s; using default montage %s."),
				*ToString_ReactType(ReactType), *ToString_Direction(Dir), *GetNameSafe(DefaultHitReactMontage));
		}
	}

	/*
		// Evaluate chooser (e.g. CT_HitReact_Base): top level by ReactType, nested by HitDirection -> AnimMontage
		if (HitReactChooser)
		{
			FOSHitReacts Context;
			Context.HitDirection = Dir;
			Context.ReactDirection = Dir;
			Context.ReactType = ReactType;
			// Limb left at default HEAD unless you add hit-location data later

			if (UAnimMontage* Chosen = OSChooser::Evaluate<UAnimMontage>(HitReactChooser, Context))
			{
				MontageToPlay = Chosen;
			}
			else
			{
				// Chooser returned nothing (e.g. Heavy row: ensure nested result is AnimMontage, not Animation Sequence)
				UE_LOG(LogOSGASHitReact, Verbose, TEXT("[HitReact] Chooser returned no montage for ReactType=%s Dir=%s; using default montage %s."),
					*ToString_ReactType(ReactType), *ToString_Direction(Dir), *GetNameSafe(DefaultHitReactMontage));
			}
		}
		*/

	if (!MontageToPlay)
	{
		UE_LOG(LogOSGASHitReact, Warning, TEXT("[HitReact] No montage selected: ReactType=%s Dir=%s Chooser=%s Default=%s. Set Default Hit React Montage on the ability as fallback."),
			*ToString_ReactType(ReactType), *ToString_Direction(Dir),
			*GetNameSafe(HitReactChooser), *GetNameSafe(DefaultHitReactMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (TriggerEventData)
	{
		const FVector Impact = HitForDir.bBlockingHit ? HitForDir.ImpactPoint : Avatar->GetActorLocation();
		UE_LOG(LogOSGASHitReact, Log, TEXT("[HitReact] EventMagnitude=%.1f -> ReactType=%s Dir=%s Montage=%s Impact=%s Instigator=%s Target=%s"),
			TriggerEventData->EventMagnitude,
			*ToString_ReactType(ReactType),
			*ToString_Direction(Dir),
			*GetNameSafe(MontageToPlay),
			*Impact.ToString(),
			*GetNameSafe(TriggerEventData->Instigator),
			*GetNameSafe(TriggerEventData->Target));
	}
#endif

	UE_LOG(LogOSGASHitReact, Verbose, TEXT("[HitReact] Playing montage %s (EventMagnitude=%.2f)"),
		*GetNameSafe(MontageToPlay),
		TriggerEventData ? TriggerEventData->EventMagnitude : -1.f);

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay, 1.f, NAME_None, true);

	MontageTask->OnCompleted.AddDynamic(this, &UGA_OSHitReaction::OSEndAbility);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_OSHitReaction::OSEndAbility);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_OSHitReaction::OSCancelAbility);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_OSHitReaction::OSCancelAbility);
	MontageTask->ReadyForActivation();
}

void UGA_OSHitReaction::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	MontageTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	
	// Guard break cancel removed — Event_GuardBreak is an event tag, not an asset tag.
	// CancelAbilities matches against asset tags, so this was always a no-op.
	// Guard break state is managed by ExecCalc + GE tags, not a cancellable ability.
}

