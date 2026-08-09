#pragma once
/* Base Grimoire component: class-selection system that grants a melee and projectile ability.
   Ability levels scale with aura via GAS ability levels.
   Concrete grimoires subclass this and configure their two abilities, stat effects, and identity.
   Server-authoritative: only the server grants/revokes abilities. */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "OSBaseGrimoire.generated.h"

class UOSGameplayAbility;
class UGameplayEffect;
class UAbilitySystemComponent;
class AOSCharacter;

UENUM(BlueprintType)
enum class EOSGrimoireQuadrant : uint8
{
	Pressure    UMETA(DisplayName = "Pressure / Attrition"),
	Counter     UMETA(DisplayName = "Counter / Reactive"),
	Zoning      UMETA(DisplayName = "Zoning / Range"),
	Evasive     UMETA(DisplayName = "Evasive / Disruptive")
};

UCLASS(Abstract)
class ONSIGHT_API UOSBaseGrimoire : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSBaseGrimoire();

	// --- Identity ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grimoire|Identity")
	FText GrimoireName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grimoire|Identity")
	FText Subtitle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grimoire|Identity")
	EOSGrimoireQuadrant Quadrant;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grimoire|Identity")
	FGameplayTag TacticalStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grimoire|Identity")
	FLinearColor AuraColor;

	// --- Granted Abilities ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grimoire|Abilities")
	TSubclassOf<UOSGameplayAbility> MeleeAbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grimoire|Abilities")
	TSubclassOf<UOSGameplayAbility> ProjectileAbilityClass;

	/* Passive abilities granted per aura tier. Index 0 = Tier 1, Index 1 = Tier 2, etc.
	   These are passive abilities (bActivateAbilityOnGranted) that listen for gameplay events
	   and apply grimoire-specific mechanics.
	   Granted when the player reaches the tier, revoked if they drop below. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grimoire|Abilities")
	TArray<TSubclassOf<UOSGameplayAbility>> PassiveAbilities;

	// --- Stat Modifiers ---

	/* Applied on equip, removed on unequip. Base stat adjustments for this grimoire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grimoire|Stats")
	TArray<TSubclassOf<UGameplayEffect>> BaseStatEffects;

	// --- Aura Tier Thresholds ---

	/* Aura percentage thresholds that map to GAS ability levels.
	   Index 0 = level 1 threshold, index 1 = level 2 threshold, etc.
	   Default: 0.0 (level 1), 0.25 (level 2), 0.5 (level 3), 1.0 (level 4). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grimoire|Aura")
	TArray<float> AuraTierThresholds;

	/* Equip this grimoire on the owning character. Grants abilities at level 1. Server only. */
	UFUNCTION(BlueprintCallable, Category = "Grimoire")
	void Equip();

	/* Unequip this grimoire. Revokes all granted abilities and effects. Server only. */
	UFUNCTION(BlueprintCallable, Category = "Grimoire")
	void Unequip();

	/* Call when the character's aura value changes. Updates GAS ability levels. */
	UFUNCTION(BlueprintCallable, Category = "Grimoire")
	void UpdateAura(float AuraPercent);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Grimoire")
	int32 GetCurrentAbilityLevel() const { return CurrentAbilityLevel; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Grimoire")
	bool IsEquipped() const { return bIsEquipped; }

protected:
	virtual void BeginPlay() override;

	/* Override for custom behavior when ability level changes. */
	UFUNCTION(BlueprintNativeEvent, Category = "Grimoire")
	void OnAbilityLevelChanged(int32 OldLevel, int32 NewLevel);

	/* Override for custom behavior on equip. Called after base equip logic. */
	UFUNCTION(BlueprintNativeEvent, Category = "Grimoire")
	void OnEquipped();

	/* Override for custom behavior on unequip. Called before base unequip logic. */
	UFUNCTION(BlueprintNativeEvent, Category = "Grimoire")
	void OnUnequipped();

private:
	UPROPERTY()
	int32 CurrentAbilityLevel;

	UPROPERTY()
	bool bIsEquipped;

	// --- Tracking handles for cleanup ---

	FGameplayAbilitySpecHandle MeleeHandle;
	FGameplayAbilitySpecHandle ProjectileHandle;
	TArray<FGameplayAbilitySpecHandle> PassiveHandles;
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	// --- Internal helpers ---

	AOSCharacter* GetOwningCharacter() const;
	UAbilitySystemComponent* GetASC() const;

	int32 CalcAbilityLevel(float AuraPercent) const;
	void SetAbilityLevels(int32 NewLevel);
	void GrantPassivesUpToTier(int32 TierIndex);
	void RevokePassivesAboveTier(int32 TierIndex);
	void RevokeAllPassives();
	void ApplyEffects(const TArray<TSubclassOf<UGameplayEffect>>& Effects);
	void RemoveAllAppliedEffects();
};
