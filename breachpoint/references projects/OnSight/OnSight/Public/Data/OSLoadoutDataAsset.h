#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayEffectTypes.h"
#include "OSLoadoutDataAsset.generated.h"

class UAbilitySystemComponent;
class AActor;
class UOSGameplayAbility;
class UOSGameplayEffect;

/** Optional input routing on a granted startup ability. */
USTRUCT(BlueprintType)
struct FOSLoadoutAbilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	TSubclassOf<UOSGameplayAbility> AbilityClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	FGameplayTag InputTag;
};

/** Gameplay effect with level for startup application. */
USTRUCT(BlueprintType)
struct FOSLoadoutEffectEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	TSubclassOf<UOSGameplayEffect> EffectClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	float Level = 1.f;
};

/**
 * Single authored loadout for a class/character preset (DataAsset).
 * Attribute baseline: modifiers must target UOSAttributeSet (same contract as UGE_OSPlayerDefaultAttributes).
 * Bulk: effects, abilities with optional input tags, loose tags.
 * Named: fixed combat kit slots for input/gameplay contracts.
 */
USTRUCT(BlueprintType)
struct FOSGameplayLoadoutDefinition
{
	GENERATED_BODY()

	/**
	 * Applied first on respawn/loadout reapply. Use UGE_OSPlayerDefaultAttributes or a BP child that
	 * overrides UOSAttributeSet attributes (Health, Stamina, MovementSpeed multiplier, etc.).
	 * New UOSLoadoutDataAsset instances default this to UGE_OSPlayerDefaultAttributes in C++.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Init")
	TSubclassOf<UOSGameplayEffect> InitAttributeEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Granted")
	TArray<FOSLoadoutEffectEntry> GrantedGameplayEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Granted")
	TArray<FOSLoadoutAbilityEntry> GrantedAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Granted")
	FGameplayTagContainer GrantedGameplayTags;

	// --- Named kit (explicit slots; no FName map) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Actions")
	TSubclassOf<UOSGameplayAbility> MobilityAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Actions")
	TSubclassOf<UOSGameplayAbility> GrabAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Actions")
	TSubclassOf<UOSGameplayAbility> BlockAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Actions")
	TSubclassOf<UOSGameplayAbility> OffensiveLightAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Actions")
	TSubclassOf<UOSGameplayAbility> OffensiveHeavyAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Actions")
	TSubclassOf<UOSGameplayAbility> JumpAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Class")
	TSubclassOf<UOSGameplayAbility> ClassAbility1 = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Class")
	TSubclassOf<UOSGameplayAbility> ClassAbility2 = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Class")
	TSubclassOf<UOSGameplayAbility> ClassAbility3 = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout|Class")
	TSubclassOf<UOSGameplayAbility> ClassAbility4 = nullptr;

	/**
	 * Apply this definition to an ASC (server). Caller clears prior tracked grants before calling.
	 * Out* arrays receive new handles/tags to track for the next teardown (e.g. PlayerState-owned loadout).
	 */
	void GiveToAbilitySystemComponent(
		UAbilitySystemComponent* ASC,
		AActor* AvatarActor,
		AActor* EffectContextFallback,
		TArray<FGameplayAbilitySpecHandle>& OutGrantedAbilities,
		TArray<FActiveGameplayEffectHandle>& OutGrantedEffects,
		FGameplayTagContainer& OutCachedLooseTags,
		float ApplyLevel = 1.f) const;
};

UCLASS(BlueprintType)
class ONSIGHT_API UOSLoadoutDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UOSLoadoutDataAsset();

	/** Delegates to Definition.GiveToAbilitySystemComponent (convenience on the asset). */
	void GiveToAbilitySystemComponent(
		UAbilitySystemComponent* ASC,
		AActor* AvatarActor,
		AActor* EffectContextFallback,
		TArray<FGameplayAbilitySpecHandle>& OutGrantedAbilities,
		TArray<FActiveGameplayEffectHandle>& OutGrantedEffects,
		FGameplayTagContainer& OutCachedLooseTags,
		float ApplyLevel = 1.f) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	FOSGameplayLoadoutDefinition Definition;
};
