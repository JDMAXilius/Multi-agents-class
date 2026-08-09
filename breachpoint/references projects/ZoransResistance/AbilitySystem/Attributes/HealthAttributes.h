// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ZoransAttributeSet.h"
#include "HealthAttributes.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class ZORANSRESISTANCE_API UHealthAttributes : public UZoransAttributeSet
{
	GENERATED_BODY()

public:
	// Attribute Set Overrides.
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// Used to create a local copy of Damage which is then subtracted from Current Health.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", meta = (HideFromLevelInfos))
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UHealthAttributes, Damage)

	// Used to create a local copy of Healing which is then added to Current Health.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", meta = (HideFromLevelInfos))
	FGameplayAttributeData Healing;
	ATTRIBUTE_ACCESSORS(UHealthAttributes, Healing)
	
	// Holds the current value for Health.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_CurrentHealth)
	FGameplayAttributeData CurrentHealth;
	ATTRIBUTE_ACCESSORS(UHealthAttributes, CurrentHealth)

	// Holds the value for Maximum Health.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_MaximumHealth)
	FGameplayAttributeData MaximumHealth;
	ATTRIBUTE_ACCESSORS(UHealthAttributes, MaximumHealth)

	// Holds the value for Health Regen Rate.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_HealthRegenRate)
	FGameplayAttributeData HealthRegenRate;
	ATTRIBUTE_ACCESSORS(UHealthAttributes, HealthRegenRate)

	// Holds the current value for Shield.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_CurrentShield)
	FGameplayAttributeData CurrentShield;
	ATTRIBUTE_ACCESSORS(UHealthAttributes, CurrentShield)

	// Holds the value for Maximum Shield.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_MaximumShield)
	FGameplayAttributeData MaximumShield;
	ATTRIBUTE_ACCESSORS(UHealthAttributes, MaximumShield)

	// Holds the value for Shield Regen Rate.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_ShieldRegenRate)
	FGameplayAttributeData ShieldRegenRate;
	ATTRIBUTE_ACCESSORS(UHealthAttributes, ShieldRegenRate)

protected:
	UFUNCTION()
	virtual void OnRep_CurrentHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaximumHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_HealthRegenRate(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_CurrentShield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaximumShield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_ShieldRegenRate(const FGameplayAttributeData& OldValue);
};