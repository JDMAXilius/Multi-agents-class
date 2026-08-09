// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ZoransAttributeSet.h"
#include "CharacterAttributes.generated.h"

UCLASS()
class ZORANSRESISTANCE_API UCharacterAttributes : public UZoransAttributeSet
{
	GENERATED_BODY()

public:
	
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(BlueprintReadOnly, Category = "Character Attribute Set", ReplicatedUsing = OnRep_CurrentKineticCharge)
	FGameplayAttributeData CurrentKineticCharge;
	ATTRIBUTE_ACCESSORS(UCharacterAttributes, CurrentKineticCharge)
	
	UPROPERTY(BlueprintReadOnly, Category = "Character Attribute Set", ReplicatedUsing = OnRep_MaximumKineticCharge)
	FGameplayAttributeData MaximumKineticCharge;
	ATTRIBUTE_ACCESSORS(UCharacterAttributes, MaximumKineticCharge)

	UPROPERTY(BlueprintReadOnly, Category = "Character Attribute Set", ReplicatedUsing = OnRep_KineticChargeRegen)
	FGameplayAttributeData KineticChargeRegen;
	ATTRIBUTE_ACCESSORS(UCharacterAttributes, KineticChargeRegen)

	UPROPERTY(BlueprintReadOnly, Category = "Character Attribute Set", ReplicatedUsing = OnRep_ThrowdownCharge)
	FGameplayAttributeData ThrowdownCharge;
	ATTRIBUTE_ACCESSORS(UCharacterAttributes, ThrowdownCharge)

protected:
	
	UFUNCTION()
	virtual void OnRep_CurrentKineticCharge(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaximumKineticCharge(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_KineticChargeRegen(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	virtual void OnRep_ThrowdownCharge(const FGameplayAttributeData& OldValue);
};