#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BNAttributeSet.generated.h"

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
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

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
	 *  beats two agreeing. Not an attribute: no GE ever needs to modify it. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Shield")
	float ShieldRechargeDelay = 4.f;

	UPROPERTY(ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, MoveSpeed)

	/** Sprint's speed factor. An attribute, not a literal, so the sprint GE's magnitude captures
	 *  it and a tuning change never touches ability code. */
	UPROPERTY(ReplicatedUsing = OnRep_SprintSpeedMultiplier)
	FGameplayAttributeData SprintSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, SprintSpeedMultiplier)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldShield);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);

	UFUNCTION()
	void OnRep_SprintSpeedMultiplier(const FGameplayAttributeData& OldSprintSpeedMultiplier);
};
