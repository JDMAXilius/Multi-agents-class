#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "BRPlayerState.generated.h"

class UBRAbilitySystemComponent;
class UBRAttributeSet;

UCLASS(meta = (DisplayName = "BR Player State"))
class BREACHPOINT_API ABRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABRPlayerState(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UBRAbilitySystemComponent* GetBRAbilitySystemComponent() const { return AbilitySystemComponent; }

	const UBRAttributeSet* GetBRAttributeSet() const { return AttributeSet; }

	virtual void PostInitializeComponents() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Breachpoint|Abilities")
	TObjectPtr<UBRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UBRAttributeSet> AttributeSet;
};
