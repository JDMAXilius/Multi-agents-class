// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Replicated Health/Stamina/Aura and movement. PostGameplayEffectExecute fires events (Death, GuardBreak, HitReact) and broadcasts for UI.

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Data/OSHitDamageContext.h"
#include "OSAttributeSet.generated.h"

/** Attribute set for character. Damage/GuardBreak are meta (consumed in PostExecute); do not replicate. */

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)


class UOSAbilitySystemComponent;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAttributeHealthChanged, float, float, const FOSDeathEventInfo&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAttributeStaminaChanged, float, float);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAttributeAuraChanged, float, float);

UCLASS()
class ONSIGHT_API UOSAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UOSAttributeSet();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
	
	
	
	//Attributes
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Current | Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, Health)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Current | Attributes")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, Stamina)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Aura, Category = "Current | Attributes")
	FGameplayAttributeData Aura;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, Aura)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Max | Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, MaxHealth)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Max | Attributes")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, MaxStamina)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxAura, Category = "Max | Attributes")
	FGameplayAttributeData MaxAura;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, MaxAura)

	// Movement
	// - MoveSpeedBase: units/sec (your "base walk speed")
	// - MovementSpeed: multiplier (1.0 = normal)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedBase, Category = "Movement | Attributes")
	FGameplayAttributeData MoveSpeedBase;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, MoveSpeedBase)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MovementSpeed, Category = "Movement | Attributes")
	FGameplayAttributeData MovementSpeed;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, MovementSpeed)

	// Meta attributes (not replicated). These are consumed in PostGameplayEffectExecute.
	UPROPERTY(BlueprintReadOnly, Category = "Meta | Attributes")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, Damage)

	// Meta flag (not replicated). Set by damage ExecCalc when a block causes guard break.
	UPROPERTY(BlueprintReadOnly, Category = "Meta | Attributes")
	FGameplayAttributeData GuardBreak;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, GuardBreak)

	// Meta flag (not replicated). Set by damage ExecCalc when block fully absorbs.
	UPROPERTY(BlueprintReadOnly, Category = "Meta | Attributes")
	FGameplayAttributeData Recoil;
	ATTRIBUTE_ACCESSORS(UOSAttributeSet, Recoil)

	FOnAttributeHealthChanged OnAttributeHealthChanged;
	FOnAttributeStaminaChanged OnAttributeStaminaChanged;
	FOnAttributeAuraChanged OnAttributeAuraChanged;
	
private:
	void BroadcastHealth( const FOSDeathEventInfo& deathEvent );
	void BroadcastStamina();
	void BroadcastAura();
	
	
	//OnRep
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& oldHealth);
	
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& oldHealth);
	
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& oldStamina);
	
	UFUNCTION()
	void OnRep_Aura(const FGameplayAttributeData& oldAura);
	
	UFUNCTION()
	void OnRep_MaxStamina (const FGameplayAttributeData& oldStamina);
	
	UFUNCTION()
	void OnRep_MaxAura (const FGameplayAttributeData& oldAura);
	
	UFUNCTION()
	void OnRep_MoveSpeedBase(const FGameplayAttributeData& oldMoveSpeedBase);

	UFUNCTION()
	void OnRep_MovementSpeed(const FGameplayAttributeData& oldMovementSpeed);
};
