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

UCLASS()
class BREACHPOINTNEXT_API UBNAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, Health)

	UPROPERTY(ReplicatedUsing = OnRep_Shield)
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UBNAttributeSet, Shield)

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
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);

	UFUNCTION()
	void OnRep_SprintSpeedMultiplier(const FGameplayAttributeData& OldSprintSpeedMultiplier);
};
