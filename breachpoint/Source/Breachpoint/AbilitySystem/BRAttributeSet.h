// Breachpoint. THE attribute set: shields over health, one damage entry point, one death.
#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"

#include "BRAttributeSet.generated.h"

struct FGameplayEffectSpec;

DECLARE_MULTICAST_DELEGATE_FourParams(FBROnDeathSignature, AActor*, AActor*, AActor*, const FGameplayEffectSpec&);

UCLASS(meta = (DisplayName = "BR Attribute Set"))
class BREACHPOINT_API UBRAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBRAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes", ReplicatedUsing = OnRep_Shields)
	FGameplayAttributeData Shields;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, Shields);

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes", ReplicatedUsing = OnRep_MaxShields)
	FGameplayAttributeData MaxShields;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, MaxShields);

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, Health);

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, MaxHealth);

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes", ReplicatedUsing = OnRep_Grenades)
	FGameplayAttributeData Grenades;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, Grenades);

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes", ReplicatedUsing = OnRep_MaxGrenades)
	FGameplayAttributeData MaxGrenades;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, MaxGrenades);

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Attributes")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UBRAttributeSet, IncomingDamage);

	FBROnDeathSignature OnDeath;

	bool HasReportedDeath() const { return bDeathReported; }

protected:
	UFUNCTION()
	void OnRep_Shields(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxShields(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Grenades(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxGrenades(const FGameplayAttributeData& OldValue);

private:
	float ApplyIncomingDamageShieldsFirst(float RawDamage);

	void CheckForDeath(const FGameplayEffectModCallbackData& Data);

	void UpdateShieldsBrokenState();

	bool bDeathReported = false;
};
