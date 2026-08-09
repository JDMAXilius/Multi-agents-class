// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "ZoransDurationWidget.generated.h"

UCLASS()
class ZORANSRESISTANCE_API UZoransDurationWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	virtual void NativeConstruct() override;
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UAbilitySystemComponent* GetOwningAbilitySystemComponent();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDurationExpired();
	
protected:
	
	TObjectPtr<UAbilitySystemComponent> OwnerAbilitySystemComponent = nullptr;

	FGameplayTagContainer GameplayTagContainer;
};