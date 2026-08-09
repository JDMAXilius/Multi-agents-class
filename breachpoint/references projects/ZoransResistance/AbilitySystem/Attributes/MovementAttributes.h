// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ZoransAttributeSet.h"
#include "MovementAttributes.generated.h"

UCLASS()
class ZORANSRESISTANCE_API UMovementAttributes : public UZoransAttributeSet
{
	GENERATED_BODY()

public:
	
	// Attribute Set Overrides.
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	
	// Holds the current value for Movement Speed.
	UPROPERTY(BlueprintReadOnly, Category = "Movement Attribute Set", ReplicatedUsing = OnRep_CurrentMovementSpeed)
	FGameplayAttributeData CurrentMovementSpeed;
	ATTRIBUTE_ACCESSORS(UMovementAttributes, CurrentMovementSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "Movement Attribute Set", ReplicatedUsing = OnRep_WeightModifier)
	FGameplayAttributeData WeightModifier;
	ATTRIBUTE_ACCESSORS(UMovementAttributes, WeightModifier)

	UPROPERTY(BlueprintReadOnly, Category = "Movement Attribute Set", ReplicatedUsing = OnRep_JumpModifier)
	FGameplayAttributeData JumpModifier;
	ATTRIBUTE_ACCESSORS(UMovementAttributes, JumpModifier)

protected:
	
	UFUNCTION()
	virtual void OnRep_CurrentMovementSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_WeightModifier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_JumpModifier(const FGameplayAttributeData& OldValue);
};