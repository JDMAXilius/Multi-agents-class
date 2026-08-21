#pragma once

#include "CommonUserWidget.h"
#include "Templates/SubclassOf.h"
#include "UI/BNUITypes.h"
#include "BNRootLayout.generated.h"

class UBNActivatableWidget;
class UCommonActivatableWidgetStack;

/**
 * One per local player, always on screen: four activatable stacks, one per UI.Layer.* tag.
 * Layers are a STACK, not a visibility toggle — a modal pushes, it never hides the HUD, and
 * popping restores focus to the layer beneath, which is what makes gamepad back-navigation
 * work with no bespoke code. UCommonUserWidget, deliberately not activatable: the root never
 * activates, its children do.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNRootLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UCommonActivatableWidgetStack* GetLayerStack(FUITag LayerTag) const;

	UBNActivatableWidget* PushWidgetToLayer(FUITag LayerTag, TSubclassOf<UBNActivatableWidget> WidgetClass);
	void RemoveWidgetFromLayer(FUITag LayerTag, UBNActivatableWidget* Widget);
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

	/** Key is the plain FGameplayTag under the FUITag — lookups slice implicitly. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetStack>> LayerStacks;
};
