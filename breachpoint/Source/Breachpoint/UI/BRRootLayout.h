#pragma once

#include "CommonUserWidget.h"
#include "Templates/SubclassOf.h"
#include "UI/BRUITypes.h"

#include "BRRootLayout.generated.h"

class UBRActivatableWidget;
class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;

UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRRootLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UCommonActivatableWidgetStack* GetLayerStack(FUITag LayerTag) const;

	UBRActivatableWidget* PushWidgetToLayer(FUITag LayerTag, TSubclassOf<UBRActivatableWidget> WidgetClass);

	void RemoveWidgetFromLayer(FUITag LayerTag, UBRActivatableWidget* Widget);

	void ClearLayer(FUITag LayerTag);


protected:
	virtual void NativeOnInitialized() override;

private:
	void RegisterLayer(FUITag LayerTag, UCommonActivatableWidgetStack* Stack);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UCommonActivatableWidgetStack> GameLayerStack;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UCommonActivatableWidgetStack> GameMenuLayerStack;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UCommonActivatableWidgetStack> MenuLayerStack;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UCommonActivatableWidgetStack> ModalLayerStack;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetStack>> LayerStacks;
};
