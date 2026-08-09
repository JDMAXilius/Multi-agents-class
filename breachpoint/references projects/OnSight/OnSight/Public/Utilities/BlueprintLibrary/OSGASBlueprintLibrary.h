// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AttributeSet.h"
#include "Data/OSCombatTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OSGASBlueprintLibrary.generated.h"

/**
 *
 */
UCLASS()
class ONSIGHT_API UOSGASBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="OS|GAS", meta=(DisplayName="Get OS Hit Info From Context"))
	static bool GetOSHitInfo(const FGameplayEffectContextHandle& Context, FOSHitNetInfo& OutInfo);

	UFUNCTION(BlueprintPure, Category="OS|GAS", meta=(DisplayName="Get OS Clash Info From Context"))
	static bool GetOSClashInfo(const FGameplayEffectContextHandle& Context, FOSClashInfo& OutInfo);
	UFUNCTION(BlueprintPure, Category="OS|GAS", meta=(DisplayName="Get OS Socket Name From Context"))
	static bool GetOSCueSocketName(const FGameplayEffectContextHandle& Context, FName& OutInfo);

	/** Returns true if Actor's ASC has the tag resolved from TagString. Hierarchical match by default (parent tag matches any child); set bExactMatch to require the exact tag. Invalid/unregistered strings return false. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Tags", meta=(DisplayName="Has Gameplay Tag (String)"))
	static bool HasGameplayTagByString(const AActor* Actor, const FString& TagString, bool bExactMatch = false);

	/** Resolves TagString to an FGameplayTag via the tag registry. Returns an invalid tag if the string is empty or not a registered gameplay tag. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Tags", meta=(DisplayName="Find Gameplay Tag From String"))
	static FGameplayTag FindGameplayTagFromString(const FString& TagString);

	/** Returns true if Actor's ASC has any activatable ability whose class derives from AbilityClass (includes BP subclasses and C++ children). */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Abilities", meta=(DisplayName="Has Ability Of Class"))
	static bool HasAbilityOfClass(const AActor* Actor, TSubclassOf<UGameplayAbility> AbilityClass);

	/** Returns the first granted ability on Actor's ASC whose class derives from AbilityClass (includes BP subclasses and C++ children). Null if none found. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Abilities", meta=(DisplayName="Find Ability Of Class"))
	static UGameplayAbility* FindAbilityOfClass(const AActor* Actor, TSubclassOf<UGameplayAbility> AbilityClass);

	// --- Attributes ---

	/** Returns the current (post-aggregator) value of Attribute on Actor's ASC. 0 if no ASC or attribute unset. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Attributes", meta=(DisplayName="Get Attribute Value"))
	static float GetAttributeValue(const AActor* Actor, FGameplayAttribute Attribute);

	/** Returns the base (pre-aggregator) value of Attribute on Actor's ASC. 0 if no ASC or attribute unset. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Attributes", meta=(DisplayName="Get Attribute Base Value"))
	static float GetAttributeBaseValue(const AActor* Actor, FGameplayAttribute Attribute);

	// --- Effects ---

	/** Returns true if Actor's ASC has any active GameplayEffect whose owned/granted tags include EffectTag. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Effects", meta=(DisplayName="Has Active Effect With Tag"))
	static bool HasActiveEffectWithTag(const AActor* Actor, FGameplayTag EffectTag);

	/** Returns the longest remaining duration (seconds) across all active effects matching EffectTag. 0 if none; -1 for an infinite effect. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Effects", meta=(DisplayName="Get Active Effect Time Remaining"))
	static float GetActiveEffectTimeRemaining(const AActor* Actor, FGameplayTag EffectTag);

	/** Removes active effects matching EffectTag on Actor's ASC. StacksToRemove=-1 removes all stacks. Returns count removed. Authority-only by ASC rules. */
	UFUNCTION(BlueprintCallable, Category="OS|GAS|Effects", meta=(DisplayName="Remove Active Effects With Tag"))
	static int32 RemoveActiveEffectsWithTag(AActor* Actor, FGameplayTag EffectTag, int32 StacksToRemove = -1);

	// --- Abilities ---

	/** Attempts to activate any granted ability whose asset tags match TagContainer. Mirrors ASC::TryActivateAbilitiesByTag. */
	UFUNCTION(BlueprintCallable, Category="OS|GAS|Abilities", meta=(DisplayName="Try Activate Ability By Tag"))
	static bool TryActivateAbilityByTag(AActor* Actor, FGameplayTagContainer TagContainer, bool bAllowRemoteActivation = true);

	/** Cancels any active ability whose asset tags match TagContainer. Hierarchical — parent tag cancels children. */
	UFUNCTION(BlueprintCallable, Category="OS|GAS|Abilities", meta=(DisplayName="Cancel Abilities By Tag"))
	static void CancelAbilitiesByTag(AActor* Actor, FGameplayTagContainer TagContainer);

	/** Returns every currently-running ability instance on Actor's ASC whose class derives from AbilityClass. Impure (BlueprintCallable) to avoid re-iterating the spec list on every output-pin read — cache the result to a local if you need it multiple times. */
	UFUNCTION(BlueprintCallable, Category="OS|GAS|Abilities", meta=(DisplayName="Get Active Abilities Of Class"))
	static TArray<UGameplayAbility*> GetActiveAbilitiesOfClass(const AActor* Actor, TSubclassOf<UGameplayAbility> AbilityClass);

	/** Returns true if Actor has a currently-running (not just granted) ability of AbilityClass or a subclass. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Abilities", meta=(DisplayName="Is Ability Active"))
	static bool IsAbilityActive(const AActor* Actor, TSubclassOf<UGameplayAbility> AbilityClass);

	// --- Tags ---

	/** Returns the full owned-tag container on Actor's ASC. Empty if no ASC. Impure (BlueprintCallable) because the container copy is not cheap on every output-pin read — cache the result to a local if you need it multiple times. */
	UFUNCTION(BlueprintCallable, Category="OS|GAS|Tags", meta=(DisplayName="Get Owned Gameplay Tags"))
	static FGameplayTagContainer GetOwnedGameplayTags(const AActor* Actor);

	/** Adds a non-replicated loose tag to Actor's ASC. Local-only — use for cosmetic/UI state, not gameplay decisions on other machines. */
	UFUNCTION(BlueprintCallable, Category="OS|GAS|Tags", meta=(DisplayName="Add Local Loose Gameplay Tag"))
	static void AddLocalLooseGameplayTag(AActor* Actor, FGameplayTag Tag);

	/** Removes a non-replicated loose tag from Actor's ASC. Local-only. */
	UFUNCTION(BlueprintCallable, Category="OS|GAS|Tags", meta=(DisplayName="Remove Local Loose Gameplay Tag"))
	static void RemoveLocalLooseGameplayTag(AActor* Actor, FGameplayTag Tag);

	// --- Context ---

	/** Returns the attack type recorded on the OS context, if present. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Context", meta=(DisplayName="Get Context Attack Type"))
	static bool GetContextAttackType(const FGameplayEffectContextHandle& Context, EOSAttackType& OutAttackType);

	/** Returns the instigator actor on the context (the one who caused the effect). Null if context is invalid. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Context", meta=(DisplayName="Get Context Instigator"))
	static AActor* GetContextInstigator(const FGameplayEffectContextHandle& Context);

	/** Returns the effect-causer actor on the context (physical cause, e.g. projectile). Null if context is invalid. */
	UFUNCTION(BlueprintPure, Category="OS|GAS|Context", meta=(DisplayName="Get Context Effect Causer"))
	static AActor* GetContextEffectCauser(const FGameplayEffectContextHandle& Context);
};
