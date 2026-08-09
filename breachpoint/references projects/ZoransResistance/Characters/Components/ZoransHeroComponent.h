// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "ZoransHeroComponent.generated.h"

class UCharacterAttributes;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FKineticChargeChangeDelegate, float, CurrentChargeValue, float, OldChargeValue, float, MaxCharge, float, ChargePercentage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKineticChargeRegenChangeDelegate, float, CurrentValue, float, OldValue);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ZORANSRESISTANCE_API UZoransHeroComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZoransHeroComponent();

	UPROPERTY(BlueprintAssignable)
	FKineticChargeChangeDelegate OnKineticChargeChange;

	UPROPERTY(BlueprintAssignable)
	FKineticChargeRegenChangeDelegate OnKineticChargeRegenChange;

	UFUNCTION(BlueprintCallable)
	void InitHeroComponent(AActor* InOwningActor);

	bool IsComponentInitialized() const { return bIsInitialized; }
	
protected:
	
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	mutable const UCharacterAttributes* CharacterAttributes;

	bool bIsInitialized = false;
	
	FDelegateHandle CurrentChargeChangeDelegate;
	FDelegateHandle MaximumChargeChangeDelegate;
	FDelegateHandle KineticChargeRegenChangeDelegate;
	
	void On_CurrentChargeChanged(const FOnAttributeChangeData& Data) const;
	void On_MaxChargeChanged(const FOnAttributeChangeData& Data) const;
	void On_KineticChargeRegenChanged(const FOnAttributeChangeData& Data) const;

	virtual void DestroyComponent(bool bPromoteChildren) override;	
};