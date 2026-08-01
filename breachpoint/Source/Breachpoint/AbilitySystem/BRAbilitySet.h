// Breachpoint. The grant unit: soft ability classes + InputTags, granted and revoked as a set.

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRAbilitySet.generated.h"

class UAbilitySystemComponent;
class UBRGameplayAbility;
class UGameplayEffect;

/**
 * One ability in a set: WHAT to grant, and WHICH InputTag activates it.
 *
 * The class reference is SOFT (`TSoftClassPtr`) — law 3, "asset refs in data are soft". A hard
 * reference here would pull every ability class (and its montages, cues and target actors) into
 * memory the moment any set that names it is loaded.
 */
USTRUCT(BlueprintType)
struct FBRAbilitySetEntry
{
	GENERATED_BODY()

	/** The ability class to grant. Soft: resolved at grant time, on the server. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet")
	TSoftClassPtr<UBRGameplayAbility> Ability;

	/** Ability level. 1 unless something in this game ever levels, which nothing does. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet")
	int32 AbilityLevel = 1;

	/**
	 * The InputTag that activates it, stamped onto the spec's DYNAMIC SOURCE TAGS at grant time.
	 *
	 * This is the entire binding mechanism (§3.2: "abilities are bound by tag, not by binding
	 * code"). `UBRAbilitySystemComponent::AbilityInputTagPressed` matches against exactly this.
	 * Empty is legal and means "not activated by input" — a passive or event-triggered ability.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/** One effect in a set: applied on grant, removed on revoke. Passives, auras, stat blocks. */
USTRUCT(BlueprintType)
struct FBRAbilitySetEffectEntry
{
	GENERATED_BODY()

	/** The effect class to apply. Soft, for the same reason as the ability. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet")
	TSoftClassPtr<UGameplayEffect> Effect;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet")
	float EffectLevel = 1.f;
};

/**
 * UBRAbilitySet — the unit in which abilities are granted and revoked (§3.3).
 *
 * A weapon, a loadout or a role names ONE of these; equipping grants it and un-equipping revokes
 * exactly what was granted, by handle. That is the property that matters: revoking by handle
 * cannot remove someone else's ability and cannot leave one behind, which is what makes weapon
 * swapping and respawn survivable. Granting by class and revoking by class does both wrong the
 * first time two sources share an ability.
 *
 * WHY THE HANDLES ARE OUT-PARAMETERS RATHER THAN STORED HERE: this is a DataAsset. It is shared,
 * const, and may be granted to eight fighters at once; storing handles on it would be storing
 * per-fighter state on a shared asset, which is the classic version of this bug. The caller owns
 * the handles because the caller owns the fighter.
 *
 * THIS SIGNATURE MATCHES the one `UBREquipmentComponent` (BP03 step 1) documented and is intended
 * to be called from its `GrantAbilitySetForSlot` verbatim.
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "BR Ability Set"))
class BREACHPOINT_API UBRAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Grant every ability and effect in this set to ASC. SERVER ONLY — granting on a client
	 * produces a spec the server never sees, which then fails to activate for reasons that look
	 * like input bugs.
	 *
	 * @param SourceObject      the object the abilities came from (the `UBRWeaponInstance` for a
	 *                          weapon set), reachable from the spec so a fire ability finds its row
	 *                          and its ammo without a second lookup.
	 * @param OutAbilityHandles appended to, never cleared — the caller may accumulate.
	 * @param OutEffectHandles  appended to, never cleared.
	 *
	 * Soft classes are resolved SYNCHRONOUSLY here (this is a load point: equip). A class that was
	 * not already resident is logged at Warning with its name, so a hitch on equip has a cause in
	 * the log rather than a shrug.
	 */
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject, TArray<FGameplayAbilitySpecHandle>& OutAbilityHandles, TArray<FActiveGameplayEffectHandle>& OutEffectHandles) const;

	/**
	 * Revoke exactly the handles a previous GiveToAbilitySystem produced, and empty the arrays.
	 * Static because revoking needs nothing from the set — which is the point: the set that granted
	 * it may already have been unloaded when the weapon is dropped.
	 */
	static void TakeFromAbilitySystem(UAbilitySystemComponent* ASC, TArray<FGameplayAbilitySpecHandle>& AbilityHandles, TArray<FActiveGameplayEffectHandle>& EffectHandles);

	/** The abilities. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet", meta = (TitleProperty = "Ability"))
	TArray<FBRAbilitySetEntry> Abilities;

	/** The effects applied for as long as the set is granted. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|AbilitySet", meta = (TitleProperty = "Effect"))
	TArray<FBRAbilitySetEffectEntry> Effects;

#if WITH_EDITOR
	/** Catches the two authoring mistakes that produce silent no-ops: a null class, a duplicate InputTag. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
