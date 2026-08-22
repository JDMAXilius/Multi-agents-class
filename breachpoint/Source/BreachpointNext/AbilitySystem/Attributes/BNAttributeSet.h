#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/HitResult.h"
#include "BNAttributeSet.generated.h"

/**
 * The last damage this fighter took — AUTHORITY-ONLY, never replicated, refreshed by every landed
 * hit at the one reaction point. It exists because the instigator used to reach
 * PostGameplayEffectExecute inside the spec's context, get printed in the log line, and then be
 * thrown away — so by the time anyone heard about a death, who caused it was gone. Kill credit
 * reads it at death; the hit-reaction packet reads its HitResult for direction on every hit.
 *
 * The attribute set RECORDS this; it still never talks to game flow. The death GA is what reads.
 */
class APlayerState;

struct FBNLastDamage
{
	/** Weak on purpose: a grenade's thrower can disconnect while it is in flight, and a dangling
	 *  killer must degrade to "died", never crash the credit. */
	TWeakObjectPtr<AActor> Instigator;

	/** The killer's PLAYER STATE, resolved at capture — and the one that actually pays the credit.
	 *  Instigator holds the killer's PAWN, and a pawn is destroyed by its own player's respawn: a
	 *  grenade thrown three seconds before its thrower died would find a dangling pointer and score
	 *  nothing, printing a real kill as world damage. A PlayerState outlives every pawn its player
	 *  ever wears, so it survives the window the pawn cannot. */
	TWeakObjectPtr<APlayerState> InstigatorPlayerState;

	FHitResult Hit;

	float Amount = 0.f;

	/** WHAT did it — a weapon ROW NAME, or one of BNDamageSource's rowless causes (Melee,
	 *  Grenade). NAME_None for damage that came through the door without a cause (the cheat
	 *  manager, world damage), which the feed renders as no glyph rather than a guess. */
	FName SourceName;
};

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	/** Meaningful on the AUTHORITY only — the capture happens in PostGameplayEffectExecute, which
	 *  instant GEs run nowhere else. A client reading this gets a default-constructed nothing. */
	const FBNLastDamage& GetLastDamage() const { return LastDamage; }

	/** SERVER-ONLY META attribute, deliberately NOT replicated and never read by anything: the
	 *  damage GE writes it, PostGameplayEffectExecute drains it into Shield then Health and zeroes
	 *  it in the same call. It is the whole of this wave's pipeline — no execution calculation,
	 *  no mitigation — and the door that writes it is AbilitySystem/Effects/BNDamage. */
	UPROPERTY()
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, IncomingDamage)

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, Health)

	UPROPERTY(ReplicatedUsing = OnRep_Shield)
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, Shield)

	/** The ceilings, and their absence was a real hole: nothing knew what "full" meant. Without a
	 *  max there is no bar to draw (a fraction needs a denominator), no way to say when a shield
	 *  has finished recharging, and no clamp — an overheal or an over-recharge climbed forever.
	 *  Replicated because the UI reads them on every machine, not just the server's. */
	UPROPERTY(ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, MaxHealth)

	UPROPERTY(ReplicatedUsing = OnRep_MaxShield)
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, MaxShield)

	/** Seconds after the last landed damage before the shield starts coming back. THE tuning knob
	 *  of the whole dance. It lives here because this is what applies the window, and one owner
	 *  beats two agreeing. Not an attribute: no GE ever needs to modify it.
	 *
	 *  2.5, not a number I picked: the project already decided it. `gas-purity/SKILL.md` §2 calls
	 *  State.Combat.RecentDamage "the 2.5 s regen gate tag" and says regen "starts after 2.5 s".
	 *  I had written 4 without checking, which would have quietly re-tuned a settled design. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Shield")
	float ShieldRechargeDelay = 2.5f;

	UPROPERTY(ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, MoveSpeed)

	/** Sprint's speed factor. An attribute, not a literal, so the sprint GE's magnitude captures
	 *  it and a tuning change never touches ability code. */
	UPROPERTY(ReplicatedUsing = OnRep_SprintSpeedMultiplier)
	FGameplayAttributeData SprintSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, SprintSpeedMultiplier)

	/** ADS's speed factor, sprint's pattern exactly. 0.4167 is measured, not chosen: the
	 *  reference's AimWalkSpeed 250 over its base 600 (MyCharacter.h:300,297). */
	UPROPERTY(ReplicatedUsing = OnRep_ADSSpeedMultiplier)
	FGameplayAttributeData ADSSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, ADSSpeedMultiplier)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldShield);

	/** See GetLastDamage(). Plain member, no UPROPERTY: never replicated, never serialized. */
	FBNLastDamage LastDamage;

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);

	UFUNCTION()
	void OnRep_SprintSpeedMultiplier(const FGameplayAttributeData& OldSprintSpeedMultiplier);

	UFUNCTION()
	void OnRep_ADSSpeedMultiplier(const FGameplayAttributeData& OldADSSpeedMultiplier);
};
