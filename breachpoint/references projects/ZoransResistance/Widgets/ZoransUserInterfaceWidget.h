// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"
#include "ZoransUserInterfaceWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWidgetTickDelegate, float, DeltaTime);

UCLASS()
class ZORANSRESISTANCE_API UZoransUserInterfaceWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	
	virtual void NativeConstruct() override;
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UZoransAbilitySystemComponent* GetOwningAbilitySystemComponent();

	UFUNCTION(BlueprintImplementableEvent)
	void OnRefreshWidget();
	
	UPROPERTY(BlueprintAssignable)
	FWidgetTickDelegate WidgetTickDelegate;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "User Interface", meta=(DevelopmentOnly))
	void DevelopmentOnlyFunction();


	
protected:
	
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "User Interface")
	void OnWidgetTick(float DeltaTime);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	TWeakObjectPtr<UZoransAbilitySystemComponent> OwnerAbilitySystemComponent = nullptr;
};