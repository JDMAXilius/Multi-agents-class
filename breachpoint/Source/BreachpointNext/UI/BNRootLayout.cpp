#include "UI/BNRootLayout.h"
#include "BreachpointNext.h"
#include "UI/BNActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UBNRootLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	const FBNUITags& Tags = FBNUITags::Get();
	RegisterLayer(Tags.Layer_Game, GameLayerStack);
	RegisterLayer(Tags.Layer_GameMenu, GameMenuLayerStack);
	RegisterLayer(Tags.Layer_Menu, MenuLayerStack);
	RegisterLayer(Tags.Layer_Modal, ModalLayerStack);
}

void UBNRootLayout::RegisterLayer(FUITag LayerTag, UCommonActivatableWidgetStack* Stack)
{
	// A missing stack is the WBP not matching the class — BindWidget already failed the load
	// loudly; the guard here keeps a half-built asset from crashing the map for the rest.
	if (Stack)
	{
		LayerStacks.Add(LayerTag, Stack);
	}
}

UCommonActivatableWidgetStack* UBNRootLayout::GetLayerStack(FUITag LayerTag) const
{
	const TObjectPtr<UCommonActivatableWidgetStack>* Found = LayerStacks.Find(LayerTag);
	return Found ? Found->Get() : nullptr;
}

UBNActivatableWidget* UBNRootLayout::PushWidgetToLayer(FUITag LayerTag, TSubclassOf<UBNActivatableWidget> WidgetClass)
{
	UCommonActivatableWidgetStack* Stack = GetLayerStack(LayerTag);
	if (!Stack || !WidgetClass)
	{
		UE_LOG(LogBN, Warning, TEXT("BNUI: push to layer %s refused — %s."),
			*LayerTag.ToString(), !Stack ? TEXT("no stack registered for that tag") : TEXT("null widget class"));
		return nullptr;
	}

	return Stack->AddWidget<UBNActivatableWidget>(WidgetClass);
}

void UBNRootLayout::RemoveWidgetFromLayer(FUITag LayerTag, UBNActivatableWidget* Widget)
{
	UCommonActivatableWidgetStack* Stack = GetLayerStack(LayerTag);
	if (Stack && Widget)
	{
		// Takes a REFERENCE — the compiled reference's exact call shape.
		Stack->RemoveWidget(*Widget);
	}
}

void UBNRootLayout::ClearLayer(FUITag LayerTag)
{
	if (UCommonActivatableWidgetStack* Stack = GetLayerStack(LayerTag))
	{
		Stack->ClearWidgets();
	}
}
