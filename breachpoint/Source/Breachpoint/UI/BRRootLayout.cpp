#include "UI/BRRootLayout.h"

#include "Core/BRCore.h"
#include "UI/BRActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UBRRootLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	const FBRUITags& Tags = FBRUITags::Get();
	RegisterLayer(Tags.Layer_Game, GameLayerStack);
	RegisterLayer(Tags.Layer_GameMenu, GameMenuLayerStack);
	RegisterLayer(Tags.Layer_Menu, MenuLayerStack);
	RegisterLayer(Tags.Layer_Modal, ModalLayerStack);
}

void UBRRootLayout::RegisterLayer(FUITag LayerTag, UCommonActivatableWidgetStack* Stack)
{
	if (!Stack)
	{
		return;
	}

	LayerStacks.Add(LayerTag, Stack);
}

UCommonActivatableWidgetStack* UBRRootLayout::GetLayerStack(FUITag LayerTag) const
{
	const TObjectPtr<UCommonActivatableWidgetStack>* Found = LayerStacks.Find(LayerTag);
	return Found ? Found->Get() : nullptr;
}

UBRActivatableWidget* UBRRootLayout::PushWidgetToLayer(FUITag LayerTag, TSubclassOf<UBRActivatableWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		return nullptr;
	}

	UCommonActivatableWidgetStack* Stack = GetLayerStack(LayerTag);
	if (!Stack)
	{
		return nullptr;
	}

	return Stack->AddWidget<UBRActivatableWidget>(WidgetClass);
}

void UBRRootLayout::RemoveWidgetFromLayer(FUITag LayerTag, UBRActivatableWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	if (UCommonActivatableWidgetStack* Stack = GetLayerStack(LayerTag))
	{
		Stack->RemoveWidget(*Widget);
	}
}

void UBRRootLayout::ClearLayer(FUITag LayerTag)
{
	if (UCommonActivatableWidgetStack* Stack = GetLayerStack(LayerTag))
	{
		Stack->ClearWidgets();
	}
}

