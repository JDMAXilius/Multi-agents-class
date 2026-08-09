// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Base for all OnSight abilities. Adds FOSResource cost/drain, OSCharacter/ASC helpers, and optional cancel/block tag behavior.

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Data/OSAbilityCostAndEffects.h"

#include "OSGameplayAbility.generated.h"

class UOSAttributeSet;
class UChooserTable;
class AOSCharacter;
class UAbilityTask;
class UAnimMontage;
class UOSAbilitySystemComponent;

/** Base gameplay ability: cost (BaseCosts), periodic drain (DrainPerSecond), and OwningCharacter/ASC/AttributeSet helpers. */
UCLASS()
class ONSIGHT_API UOSGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UOSGameplayAbility();

	/** Get owning character with weak pointer caching (safer than raw pointer) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Ability")
	AOSCharacter* GetOwningCharacter() const;
	
	AOSCharacter* Avatar() const;
	
	/** Get ability system component with weak pointer caching (safer than raw pointer) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Ability")
	UOSAbilitySystemComponent* GetOSAbilitySystemComponent();
	UOSAbilitySystemComponent* AbilityComponent();
	
	UAbilitySystemComponent* ASC() const;
	UOSAttributeSet* OSAttrs() const;
	
	mutable UAbilitySystemComponent* CachedASC = nullptr;
	mutable UOSAttributeSet* CachedAttrs = nullptr;
	
	// ========================================
	// RESOURCE COSTS
	// ========================================
	
	/** Initial resource cost to activate ability (e.g., Stamina, Aura) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cost")
	TArray<FOSResource> BaseCosts;
	

	/** Resource drain per second while ability is active (e.g., Stamina drain while sprinting/blocking)
	 * This will be applied as a Periodic GameplayEffect when the ability activates.
	 * The effect will be automatically removed when the ability ends.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cost")
	TArray<FOSResource> DrainPerSecond;
	
	/** Interval for periodic drain effect (default: 0.1 seconds)
	 * This sets the period of the GameplayEffect that drains resources.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cost", meta=(ClampMin="0.05", ClampMax="5.0"))
	float DrainCheckInterval = 0.1f;
	
	// ========================================
	// ABILITY LIFECYCLE
	// ========================================
	
	// Replicates K2's EndAbility functionality. Uses current ability's specs as parameters.
	// Useful for binding to delegates (e.g., AbilityTask completion callbacks)
	UFUNCTION()
	virtual void OSEndAbility();
	
	UFUNCTION()
	virtual void OSInterruptAbility(bool replicated);
	
	UFUNCTION()
	virtual void OSCancelAbility();
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, 
		const FGameplayEventData* TriggerEventData) override;

	virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	
	
	// Override GAS cost system to support BaseCosts
	// BaseCosts allows abilities to define resource costs directly (e.g., Stamina, Aura)
	// This works alongside the standard CostGE (Cost GameplayEffect) system
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, 
	                       const FGameplayAbilityActorInfo* ActorInfo, 
	                       FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, 
	                       const FGameplayAbilityActorInfo* ActorInfo,
	                       const FGameplayAbilityActivationInfo ActivationInfo) const override;
	
	
	/* Unified cooldown pipeline: assign CooldownGameplayEffectClass = UGE_OSGenericCooldown on the ability
	   (or inherit it from UGA_OSBaseMagic etc.), author CooldownTags on the BP, set CooldownDuration in
	   seconds. ApplyCooldown (below) SetByCallers the duration and adds CooldownTags via DynamicGrantedTags
	   so CheckCooldown sees them on the ASC. Per-ability C++/BP cooldown GE classes are no longer required. */

	/* Per-ability cooldown duration in seconds. 0 = no cooldown applied (ApplyCooldown is a no-op).
	   Consumed by UGE_OSGenericCooldown via SetByCaller(Data.Cooldown.Duration). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cooldown", meta=(ClampMin="0.0"))
	float CooldownDuration;

	/* Tags that CheckCooldown looks for on the ASC and that ApplyCooldown adds to the cooldown GE's
	   DynamicGrantedTags. Share a tag across abilities (e.g. Cooldown.Magic.Melee) to give them a
	   shared cooldown slot, or give each ability its own. Empty = fall back to the cooldown GE's
	   granted tags (legacy GAS default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cooldown")
	FGameplayTagContainer CooldownTags;

	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	                           const FGameplayAbilityActorInfo* ActorInfo,
	                           const FGameplayAbilityActivationInfo ActivationInfo) const override;
	
	/** If true, ability will automatically activate when granted (for passive abilities) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability")
	bool bActivateAbilityOnGranted = false;
	
	// Called when ability is granted to the actor
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	
	// Called when ability is granted and avatar is set (use for passive abilities)
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	
	// Called when ability is removed from the actor
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	
	// Called when ability ends - handles cleanup of cached pointers and timers
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
	                       const FGameplayAbilityActorInfo* ActorInfo,
	                       const FGameplayAbilityActivationInfo ActivationInfo,
	                       bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Governs whether the shared OS motion-warp notify state clears this ability's warp
	 *  targets in its NotifyEnd callback.
	 *  Default: false (clear on NotifyEnd — MW stops correcting once the authored window closes).
	 *  Combo-chain abilities override to true so Hit 2's target survives Hit 1's blend-out.
	 *  Referenced from UOSAnimNotifyState_OSTrackMotionWarpTarget::NotifyEnd. */
	virtual bool PersistsWarpTargetsAcrossNotifyEnd() const { return false; }

	// NOTE: Only magic abilities are currently using this. I want to expand it to other abilities later once this is more proven.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TArray<FOSGameplayEffectWrapper> AppliedGameplayEffects;
	void ApplyGameplayEffects(AOSCharacter* Other, const FGameplayEffectContextHandle& Context);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cues")
	FGameplayCueParameters DefaultCueParameters;

protected:
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bDebugAbility = false;
	
	virtual void GetCustomCosts(const FGameplayAbilityActorInfo* ActorInfo, TArray<FOSResource>& OutCosts) const
	{
		OutCosts.Reset();
	}

	// Helper functions for custom cost system
	bool HasResources(const TArray<FOSResource>& costs) const;
	void ApplyCosts(const TArray<FOSResource>& costs) const;

	// ========================================
	// PERIODIC RESOURCE DRAIN (GameplayEffect-based)
	// ========================================
	
	/** Apply periodic resource drain as a GameplayEffect
	 * Creates and applies an Infinite Duration Periodic GameplayEffect that drains resources.
	 * The effect will be automatically removed when the ability ends.
	 */
	void ApplyPeriodicResourceDrain();
	
	/** Remove periodic resource drain GameplayEffect */
	void RemovePeriodicResourceDrain();
	
	/** Default: native UGE_OSAbilityGenericResourceCost (SetByCaller Data.Cost.*). Override for a custom BP GE if needed. When BaseCosts or GetCustomCosts() specify positive spend and this is cleared, CheckCost fails (#97). Set Cost Gameplay Effect to the **same class** as this when using that pattern so CheckCost/ApplyCost skip the bare-spec path (avoids GetMagnitude errors). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability")
	TSubclassOf<UGameplayEffect> GenericCostGE;
	
	// Used when an ability needs to be activated when blocked (like dodge while attacking).
	// Useful when we want the blocked ability to cancel the current blocking ability when, let's say, within a cancel buffer.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meta")
	FGameplayTagContainer IgnoredTagsOnCancel;
	
	// Fires a cue with provided parameters. Will assume it's empty if not provided.
	UFUNCTION(BlueprintCallable, Category="Cues")
	void RunCueWithParams(const FGameplayTag& GameplayTag, FGameplayCueParameters CueParams = FGameplayCueParameters(), bool bTrack = false);

	// Runs a cue with default parameters that can be modified. Useful if you don't want to have to tell all cues to face the same way manually.
	UFUNCTION(BlueprintCallable, Category="Cues")
	void RunCue(const FGameplayTag& GameplayTag, bool bTrack = false);
	UFUNCTION()
	void OnEventToCueReceived(FGameplayEventData Payload);
	
	
	// Cues that the ability runs when it activates
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cues")
	TSet<FGameplayTag> OnActivateCues;
	// Cues that the ability runs when it ends
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cues")
	TSet<FGameplayTag> OnEndCues;
	// Subscribes to every Key tag as an event. Runs the appropriate Value Tag as a cue when its event fires.
	TMap<FGameplayTag, FGameplayTag> EventToCueMap;
	TSet<FGameplayTag> ActiveCues;
	
	UFUNCTION(BlueprintNativeEvent, Category="Frontend")
	void OnActivateAbility(const FGameplayAbilitySpecHandle& Handle, 
		const FGameplayAbilityActorInfo& ActorInfo, 
		const FGameplayAbilityActivationInfo& ActivationInfo, 
		const FGameplayEventData& TriggerEventData);
	
	UFUNCTION(BlueprintNativeEvent, Category="Frontend")
	void OnEndAbility(const FGameplayAbilitySpecHandle& Handle,
						   const FGameplayAbilityActorInfo& ActorInfo,
						   const FGameplayAbilityActivationInfo& ActivationInfo,
						   bool bReplicateEndAbility, bool bWasCancelled);
	
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	/** Cached owning character (weak pointer for safety) */
	mutable TWeakObjectPtr<AOSCharacter> OwningCharacter;
	
	/** Cached ability system component (weak pointer for safety) */
	mutable TWeakObjectPtr<UOSAbilitySystemComponent> OSAbilitySystemComponent;
	
	/** Handle to the active drain GameplayEffect */
	FActiveGameplayEffectHandle DrainEffectHandle;

};
