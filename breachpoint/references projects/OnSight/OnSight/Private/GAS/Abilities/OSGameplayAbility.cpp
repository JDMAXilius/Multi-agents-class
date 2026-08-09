// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Abilities/OSGameplayAbility.h"
#include "Characters/OSCharacter.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "Data/OSAbilityCostAndEffects.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Misc/DataValidation.h"
#include "Utilities/AbilityHelper.h"
#include "Data/OSGameplayTags.h"
#include "OSLogCategories.h"
#include "GAS/Effects/GE_OSAbilityGenericResourceCost.h"
#include "GAS/Effects/GE_OSGenericCooldown.h"

namespace OSAbilityCostHelpers
{
	static bool HasPositiveResourceSpend(const TArray<FOSResource>& Costs)
	{
		for (const FOSResource& C : Costs)
		{
			if (C.Attribute.IsValid() && C.Amount > 0.f)
			{
				return true;
			}
		}
		return false;
	}
}

UOSGameplayAbility::UOSGameplayAbility()
	: CooldownDuration(0.f)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// #97: Safe default so BaseCosts / GetCustomCosts spend without a Blueprint-assigned GE.
	GenericCostGE = UGE_OSAbilityGenericResourceCost::StaticClass();

	FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.State.IsDead"), false);
	if (DeadTag.IsValid())
	{
		ActivationBlockedTags.AddTag(DeadTag);
	}
}

// Avatar as AOSCharacter; cached after first successful cast from ActorInfo.
AOSCharacter* UOSGameplayAbility::GetOwningCharacter() const
{
	// Check if cached weak pointer is still valid
	if (IsValid(OwningCharacter.Get()))
	{
		return OwningCharacter.Get();
	}
	
	// Cache from avatar actor info
	if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		OwningCharacter = Cast<AOSCharacter>(AvatarActor);
		return OwningCharacter.Get();
	}
	
	return nullptr;
}

AOSCharacter* UOSGameplayAbility::Avatar() const
{
	// Legacy function - redirects to GetOwningCharacter()
	return GetOwningCharacter();
}

// ASC as UOSAbilitySystemComponent; from CurrentActorInfo when ability is active.
UOSAbilitySystemComponent* UOSGameplayAbility::GetOSAbilitySystemComponent()
{
	// Check if cached weak pointer is still valid
	if (IsValid(OSAbilitySystemComponent.Get()))
	{
		return OSAbilitySystemComponent.Get();
	}
	
	// Get from CurrentActorInfo (should always be available when ability is active)
	if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		if (UOSAbilitySystemComponent* OSASC = Cast<UOSAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()))
		{
			OSAbilitySystemComponent = OSASC;
			return OSAbilitySystemComponent.Get();
		}
	}
	
	return nullptr;
}

UOSAbilitySystemComponent* UOSGameplayAbility::AbilityComponent()
{
	// Legacy function - redirects to GetOSAbilitySystemComponent()
	return GetOSAbilitySystemComponent();
}

UAbilitySystemComponent* UOSGameplayAbility::ASC() const
{
	if (CachedASC)
		return CachedASC;

	if (const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo())
		CachedASC = Info->AbilitySystemComponent.Get();
	
	return CachedASC;
}

// Cached AttributeSet from ASC; use for reading Health/Stamina/Aura etc.
UOSAttributeSet* UOSGameplayAbility::OSAttrs() const
{
	if (CachedAttrs)
		return CachedAttrs;

	if (UAbilitySystemComponent* A = ASC())
	{
		const UOSAttributeSet* ConstAttrs = A->GetSet<UOSAttributeSet>();
		CachedAttrs = const_cast<UOSAttributeSet*>(ConstAttrs);
	}

	return CachedAttrs;
}

// End ability normally (replicate, not interrupt).
void UOSGameplayAbility::OSEndAbility()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// End as interrupt (optional replication).
void UOSGameplayAbility::OSInterruptAbility(bool replicated)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, replicated, true);
}

// Cancel ability (replicate).
void UOSGameplayAbility::OSCancelAbility()
{
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

// Commit cost/cooldown first; on failure broadcast AbilityFailedCallbacks and end. If State_InputCancelBuffer + Meta_Cancels_Input, cancel cancellable abilities then call Super::ActivateAbility.
void UOSGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                         const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                         const FGameplayEventData* TriggerEventData)
{
	// Commit ability costs/cooldowns BEFORE activation
	// This checks if the ability can be activated (resources, cooldowns, etc.)
	// CommitAbility calls CheckCost() and CheckCooldown() which we override to use BaseCosts
	// It also calls ApplyCost() and ApplyCooldown() to actually consume resources and apply cooldown
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// Notify failure (used by combat / orchestration systems). Tags are best-effort here.
		if (UOSAbilitySystemComponent* OSASC = GetOSAbilitySystemComponent())
		{
			FGameplayTagContainer FailureTags;
			const FGameplayTag CommitFailedTag = FGameplayTag::RequestGameplayTag(FName("Ability.Failure.Commit"), false);
			if (CommitFailedTag.IsValid())
			{
				FailureTags.AddTag(CommitFailedTag);
			}
			OSASC->AbilityFailedCallbacks.Broadcast(this, FailureTags);
		}

		// If commit fails (not enough resources, on cooldown, etc.), cancel activation
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	auto asc = ActorInfo->AbilitySystemComponent.Get();
	const auto& tags = FOSGameplayTags::Get();
	
	// cancels all opted-in abilities before activating. can expand this later via tags, for now, just all cancellable abilities get canceled.
	// GetAssetTags() replaces deprecated AbilityTags direct member access (UE 5.6).
	if (asc
		&& (asc->HasMatchingGameplayTag(tags.State_InputCancelBuffer) || asc->HasMatchingGameplayTag(tags.CanCancelRoot))
		&& GetAssetTags().HasTag(tags.Meta_Cancels_Input))
	{
		FGameplayTagContainer Cancellable;
		Cancellable.AddTag(tags.Meta_Cancellable);

		asc->CancelAbilities(&Cancellable, nullptr, this); 
	}
	
	
	
	// Only proceed with activation if commit succeeded
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// Calls Frontend hook
	OnActivateAbility(Handle, 
		ActorInfo? *ActorInfo : FGameplayAbilityActorInfo(),
		ActivationInfo, 
		TriggerEventData ? *TriggerEventData : FGameplayEventData());
}

bool UOSGameplayAbility::DoesAbilitySatisfyTagRequirements( const UAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags ) const
{
	
	bool ok = Super::DoesAbilitySatisfyTagRequirements(
	   AbilitySystemComponent, SourceTags, TargetTags, OptionalRelevantTags);
	
	if (ok) return true;
	
	const auto& tags = FOSGameplayTags::Get();
	
	const bool inBuffer =
	   AbilitySystemComponent.HasMatchingGameplayTag(tags.State_InputCancelBuffer)
	   || AbilitySystemComponent.GetOwnedGameplayTags().HasTag(tags.CanCancelRoot);
	// GetAssetTags() replaces deprecated AbilityTags direct member access (UE 5.6).
	const bool canCancel =
		GetAssetTags().HasTag(tags.Meta_Cancels_Input);
	
	if (inBuffer && canCancel)
	{
		// Temporarily removes ignored tags from the ability evaluation.
		if (ActivationBlockedTags.HasAny(IgnoredTagsOnCancel))
		{
			FGameplayTagContainer Blocked = ActivationBlockedTags;
			Blocked.RemoveTags(IgnoredTagsOnCancel);
			
			if (!AbilitySystemComponent.HasAnyMatchingGameplayTags(Blocked))
			{
				return true;
			}
		}
	}
	
	return false;
}


// True if CostGE passes and BaseCosts + GetCustomCosts resources are sufficient (HasResources).
bool UOSGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, 
                                   const FGameplayAbilityActorInfo* ActorInfo,
                                   FGameplayTagContainer* OptionalRelevantTags) const
{
	// UGameplayAbility::CheckCost evaluates CostGameplayEffectClass with a bare spec. BP cost GEs that use
	// SetByCaller (Data.Cost.*) log "GetMagnitude ... had not yet been set" unless magnitudes are filled here.
	// When Cost GE class == GenericCostGE, we only validate/spend via BaseCosts + ApplyCosts (primed SetByCaller).
	const bool bSkipBuiltinCostGE = GenericCostGE && CostGameplayEffectClass && CostGameplayEffectClass == GenericCostGE;

	if (!bSkipBuiltinCostGE)
	{
		if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
		{
			return false;
		}
	}

	TArray<FOSResource> CustomCosts;
	GetCustomCosts(ActorInfo, CustomCosts);

	// #97: ApplyCosts is a no-op when GenericCostGE is null — previously CheckCost could still pass and spend never happened.
	if (OSAbilityCostHelpers::HasPositiveResourceSpend(BaseCosts) || OSAbilityCostHelpers::HasPositiveResourceSpend(CustomCosts))
	{
		if (!GenericCostGE)
		{
			UE_LOG(LogOSCombat, Error,
				TEXT("UOSGameplayAbility '%s' (%s): BaseCosts or GetCustomCosts specifies resource spend but GenericCostGE is unset. ")
				TEXT("Assign your SetByCaller cost GE (same class as Cost Gameplay Effect when using that pattern)."),
				*GetName(), *GetPathName());
			if (OptionalRelevantTags)
			{
				const FGameplayTag CostFailedTag = FGameplayTag::RequestGameplayTag(FName("Ability.Cost.Failed"), false);
				if (CostFailedTag.IsValid())
				{
					OptionalRelevantTags->AddTag(CostFailedTag);
				}
			}
			return false;
		}
	}
	
	// Then check custom BaseCosts if any are defined
	// BaseCosts allows abilities to define resource costs directly in C++/Blueprint
	if (BaseCosts.Num() > 0)
	{
		if (!HasResources(BaseCosts))
		{
			// Add relevant tags for why cost check failed
			if (OptionalRelevantTags)
			{
				FGameplayTag CostFailedTag = FGameplayTag::RequestGameplayTag(FName("Ability.Cost.Failed"), false);
				if (CostFailedTag.IsValid())
				{
					OptionalRelevantTags->AddTag(CostFailedTag);
				}
			}
			return false;
		}
	}
	if (CustomCosts.Num() > 0 && !HasResources(CustomCosts))
	{
		return false;
	}
	
	return true;
}

// Per-ability cooldown tag override. Empty = fall back to UGameplayAbility's default
// (cooldown GE's granted tags), which matches existing abilities that rely on that pattern.
const FGameplayTagContainer* UOSGameplayAbility::GetCooldownTags() const
{
	if (!CooldownTags.IsEmpty())
		return &CooldownTags;
	return Super::GetCooldownTags();
}

// Unified cooldown: make a spec of CooldownGameplayEffectClass, SetByCaller Data.Cooldown.Duration,
// dynamically grant the ability's CooldownTags so CheckCooldown sees them on the ASC.
void UOSGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
                                        const FGameplayAbilityActorInfo* ActorInfo,
                                        const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (CooldownDuration <= 0.f)
		return;

	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE)
		return;

	UAbilitySystemComponent* AbilityASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AbilityASC)
		return;

	FGameplayEffectContextHandle Ctx = AbilityASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = AbilityASC->MakeOutgoingSpec(
		CooldownGE->GetClass(), GetAbilityLevel(Handle, ActorInfo), Ctx);
	if (!Spec.IsValid())
		return;

	Spec.Data->SetSetByCallerMagnitude(FOSGameplayTags::Get().Data_Cooldown_Duration, CooldownDuration);

	if (const FGameplayTagContainer* CDTags = GetCooldownTags())
	{
		if (CDTags->Num() > 0)
			Spec.Data->DynamicGrantedTags.AppendTags(*CDTags);
		else
			UE_LOG(LogOSCombat, Warning,
				TEXT("ApplyCooldown: '%s' has CooldownDuration > 0 but empty CooldownTags. CheckCooldown will not block re-activation."),
				*GetPathName());
	}

	AbilityASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

// Apply CostGE then BaseCosts and GetCustomCosts via GenericCostGE (SetByCaller Data.Cost.*).
void UOSGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
                                    const FGameplayAbilityActorInfo* ActorInfo,
                                    const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const bool bSkipBuiltinCostGE = GenericCostGE && CostGameplayEffectClass && CostGameplayEffectClass == GenericCostGE;

	if (!bSkipBuiltinCostGE)
	{
		Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	}
	
	// Then apply custom BaseCosts if any are defined
	// BaseCosts allows abilities to consume resources directly
	if (BaseCosts.Num() > 0)
	{
		ApplyCosts(BaseCosts);
	}
	TArray<FOSResource> Custom;
	GetCustomCosts(ActorInfo, Custom);
	if (Custom.Num() > 0)
		ApplyCosts(Custom);
}

// True if current attribute values >= each cost amount (Stamina/Aura/Health from OSAttrs).
bool UOSGameplayAbility::HasResources(const TArray<FOSResource>& costs) const
{
	auto attr = OSAttrs();
	if (!attr) return false;

	for (const FOSResource& cost : costs)
	{
		if (!cost.Attribute.IsValid() || cost.Amount <= 0.f)
			continue;

		float current = cost.Attribute.GetNumericValueChecked(attr);
		if (current < cost.Amount)
			return false;
	}
	return true;
}

// Applies GenericCostGE with SetByCaller magnitudes (Data.Cost.Aura/Stamina/Health) for each cost; negative = drain.
void UOSGameplayAbility::ApplyCosts(const TArray<FOSResource>& costs) const
{
	const FGameplayAbilityActorInfo* Info = CurrentActorInfo;
	if (!Info) return;

	UAbilitySystemComponent* ASC = Info->AbilitySystemComponent.Get();
	if (!ASC) return;

	if (!GenericCostGE)
	{
		if (OSAbilityCostHelpers::HasPositiveResourceSpend(costs))
		{
			UE_LOG(LogOSCombat, Error,
				TEXT("ApplyCosts: GenericCostGE null on '%s' — positive spend was configured; CheckCost should have blocked. No resources deducted."),
				*GetPathName());
		}
		return;
	}

	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GenericCostGE, 1.f, Ctx);
	if (!Spec.IsValid()) return;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	// Generic BP cost GEs often register modifiers for every Data.Cost.* channel; unset tags trip GetMagnitude errors.
	if (Tags.Data_Cost_Health.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(Tags.Data_Cost_Health, 0.f);
	}
	if (Tags.Data_Cost_Aura.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(Tags.Data_Cost_Aura, 0.f);
	}
	if (Tags.Data_Cost_Stamina.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(Tags.Data_Cost_Stamina, 0.f);
	}

	bool bAnyCost = false;
	for (const FOSResource& Cost : costs)
	{
		if (!Cost.Attribute.IsValid() || Cost.Amount <= 0.f) continue;

		const FGameplayTag Tag = AbilityHelper::MapAttributeToCostTag(Cost.Attribute);
		if (!Tag.IsValid()) continue;

		Spec.Data->SetSetByCallerMagnitude(Tag, -Cost.Amount);
		bAnyCost = true;
	}

	if (bAnyCost)
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UOSGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	
	// Clear cached pointers when ability is granted (they'll be re-cached on first use)
	CachedASC = nullptr;
	CachedAttrs = nullptr;
	OwningCharacter.Reset();
	OSAbilitySystemComponent.Reset();
}

void UOSGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	
	if (bActivateAbilityOnGranted)
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle, false);
	}
}

void UOSGameplayAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnRemoveAbility(ActorInfo, Spec);
	
	// Clear cached pointers when ability is removed
	CachedASC = nullptr;
	CachedAttrs = nullptr;
	OwningCharacter.Reset();
	OSAbilitySystemComponent.Reset();
}

void UOSGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
                                     const FGameplayAbilityActorInfo* ActorInfo,
                                     const FGameplayAbilityActivationInfo ActivationInfo,
                                     bool bReplicateEndAbility, bool bWasCancelled)
{
	// Remove periodic resource drain effect if active
	RemovePeriodicResourceDrain();

	// Notify end (combat/orchestration systems may use this to advance state machines)
	if (UOSAbilitySystemComponent* OSASC = GetOSAbilitySystemComponent())
	{
		OSASC->AbilityEndedCallbacks.Broadcast(this);
	}
	
	// Clear cached pointers when ability ends
	CachedASC = nullptr;
	CachedAttrs = nullptr;
	OwningCharacter.Reset();
	OSAbilitySystemComponent.Reset();
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	OnEndAbility(Handle, 
		ActorInfo ? *ActorInfo : FGameplayAbilityActorInfo(), 
		ActivationInfo, 
		bReplicateEndAbility, 
		bWasCancelled);
}

void UOSGameplayAbility::ApplyGameplayEffects( AOSCharacter* Other, const FGameplayEffectContextHandle& Context )
{
	if (!Context.IsValid()) return;
	
	UOSAbilitySystemComponent* OtherASC = AbilityHelper::GetOSASCFromActor(Other);
	
	for (auto const Effect : AppliedGameplayEffects)
		AbilityHelper::ApplyEffect(GetOSAbilitySystemComponent(), OtherASC, Context, Effect);
}

// ========================================
// PERIODIC RESOURCE DRAIN (GameplayEffect-based)
// ========================================

void UOSGameplayAbility::ApplyPeriodicResourceDrain()
{
	if (DrainPerSecond.Num() == 0 || DrainCheckInterval <= 0.0f)
	{
		return;
	}
	
	UOSAbilitySystemComponent* ASC = GetOSAbilitySystemComponent();
	if (!ASC || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}
	
	// Check if resources are sufficient before applying drain
	for (const FOSResource& Drain : DrainPerSecond)
	{
		if (!Drain.Attribute.IsValid() || Drain.Amount <= 0.f)
			continue;
		
		float DrainPerPeriod = Drain.Amount * DrainCheckInterval;
		float CurrentValue = ASC->GetNumericAttribute(Drain.Attribute);
		
		// If resources are insufficient, cancel the ability
		if (CurrentValue < DrainPerPeriod)
		{
			CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
			return;
		}
	}
	
	// Create a temporary GameplayEffect definition for periodic resource drain
	// Note: For production, consider creating a base PeriodicDrainEffect Blueprint
	// archetype and customizing the spec with SetByCallerMagnitude instead.
	UGameplayEffect* DrainEffectDef = NewObject<UGameplayEffect>(GetTransientPackage(), 
		FName(*FString::Printf(TEXT("DrainEffect_%s"), *GetName())));
	
	// Configure as Infinite Duration with Periodic application
	DrainEffectDef->DurationPolicy = EGameplayEffectDurationType::Infinite;
	DrainEffectDef->Period = DrainCheckInterval;
	
	// Add modifiers for each resource drain
	for (const FOSResource& Drain : DrainPerSecond)
	{
		if (!Drain.Attribute.IsValid() || Drain.Amount <= 0.f)
			continue;
		
		FGameplayModifierInfo ModInfo;
		ModInfo.Attribute = Drain.Attribute;
		ModInfo.ModifierOp = EGameplayModOp::Additive;
		// Calculate drain per period (negative value to drain)
		// ModifierMagnitude uses FScalableFloat for the value
		ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-Drain.Amount * DrainCheckInterval));
		DrainEffectDef->Modifiers.Add(ModInfo);
	}
	
	// Create effect context
	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	
	// Create spec manually - we can't use MakeOutgoingSpec with a dynamic effect object
	// So we construct the spec directly
	FGameplayEffectSpec Spec(DrainEffectDef, EffectContext, 1.0f);
	Spec.Duration = -1.0f; // Infinite duration
	Spec.Period = DrainCheckInterval;
	
	// Apply the periodic drain effect
	// Note: HandleResourceDepletedAbilities in OSAttributeSet will check resources 
	// after each periodic drain application and cancel the ability if insufficient
	DrainEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(Spec);
}

void UOSGameplayAbility::RemovePeriodicResourceDrain()
{
	if (DrainEffectHandle.IsValid())
	{
		if (UOSAbilitySystemComponent* ASC = GetOSAbilitySystemComponent())
		{
			ASC->RemoveActiveGameplayEffect(DrainEffectHandle);
		}
		DrainEffectHandle.Invalidate();
	}
}

void UOSGameplayAbility::RunCueWithParams(const FGameplayTag& GameplayTag, FGameplayCueParameters CueParams, bool bTrack)
{
	if (!ASC() || !GameplayTag.IsValid()) return;
	ASC()->AddGameplayCue(GameplayTag, CueParams);
	ActiveCues.Add(GameplayTag);
}

void UOSGameplayAbility::RunCue( const FGameplayTag& GameplayTag, bool bTrack )
{
	RunCueWithParams(GameplayTag, DefaultCueParameters, bTrack);
}

void UOSGameplayAbility::OnEventToCueReceived( FGameplayEventData Payload )
{
	if (const FGameplayTag* CueTag = EventToCueMap.Find(Payload.EventTag))
	{
		if (CueTag->IsValid())
			RunCue(*CueTag, true);
	}
}

void UOSGameplayAbility::OnActivateAbility_Implementation( const FGameplayAbilitySpecHandle& Handle,
                                                           const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo& ActivationInfo,
                                                           const FGameplayEventData& TriggerEventData )
{
	// Note: Barely even an implementation. I hate this and will change to either enums or just keep it empty instead of having a default
	// cue params that you can't even reliably edit. Would probably need help/brainstorming with other engineers.
	ACharacter* OwningChar = GetOwningCharacter();
	DefaultCueParameters.Location = OwningChar ? OwningChar->GetActorLocation() : FVector::ZeroVector;
	DefaultCueParameters.Normal = OwningChar ? OwningChar->GetActorForwardVector().RotateAngleAxis(-90.f, FVector::UpVector) : FVector::ForwardVector;
	for (const auto& cue : OnActivateCues)
		RunCue(cue, true);
	
	for (const auto& [EventTag, CueTag] : EventToCueMap)
	{
		UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag);
		if (!IsValid(EventTask)) continue;

		EventTask->EventReceived.AddDynamic(this, &UOSGameplayAbility::OnEventToCueReceived);
		EventTask->ReadyForActivation();
	}
}

void UOSGameplayAbility::OnEndAbility_Implementation( const FGameplayAbilitySpecHandle& Handle,
	const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo& ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled )
{
	for (const auto& cue : OnEndCues)
		RunCue(cue);
	
	if (ASC())
	{
		for (const auto& Cue : ActiveCues)
			ASC()->RemoveGameplayCue(Cue);
	}
    
	ActiveCues.Reset();
}

#if WITH_EDITOR
EDataValidationResult UOSGameplayAbility::IsDataValid( class FDataValidationContext& Context ) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (OSAbilityCostHelpers::HasPositiveResourceSpend(BaseCosts) && !GenericCostGE)
	{
		Context.AddError(FText::FromString(
			TEXT("BaseCosts specifies a positive resource spend but GenericCostGE is not set. Runtime activation will fail until GenericCostGE is assigned (SetByCaller cost GE, typically same as Cost Gameplay Effect).")));
		Result = EDataValidationResult::Invalid;
	}

	for (const auto& Wrapper : AppliedGameplayEffects)
	{
		if (!Wrapper.IsValid()) continue;

		const UGameplayEffect* GE = Wrapper.Effect->GetDefaultObject<UGameplayEffect>();
		if (!GE) continue;

		const EGameplayEffectDurationType GEDuration = GE->DurationPolicy;

		bool bMismatch = false;
		switch (Wrapper.Type)
		{
		case EOSEffectType::OS_INSTANT:
			bMismatch = GEDuration != EGameplayEffectDurationType::Instant; 
			break;
		case EOSEffectType::OS_DURATION:
			bMismatch = GEDuration != EGameplayEffectDurationType::HasDuration; 
			break;
		case EOSEffectType::OS_INFINITE:
			bMismatch = GEDuration != EGameplayEffectDurationType::Infinite; 
			break;
		}

		if (bMismatch)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OSGameplayEffectWrapper] Effect wrapper type mismatch: %s duration policy does not match wrapper Type."),
				*Wrapper.Effect->GetName());

			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Effect wrapper type mismatch: %s duration policy does not match wrapper Type."),
				*Wrapper.Effect->GetName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif // WITH_EDITOR
