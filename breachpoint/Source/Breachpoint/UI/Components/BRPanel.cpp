#include "UI/Components/BRPanel.h"

#include "Components/NamedSlot.h"
#include "Components/SizeBox.h"
#include "UI/Components/BRHairlineBorder.h"

void UBRPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Drive the two enumerable axes from C++ so a palette change is one edit, not 45 details
	// panels (SCREEN-MANIFEST Sec 7.4: "twelve widgets with typed hex is twelve places a rebrand
	// breaks silently").
	ApplyPanelStyle();
}

void UBRPanel::ApplyPanelStyle()
{
	if (!Frame)
	{
		return;
	}

	FBRHairlineStyle Style = Frame->GetHairlineStyle();
	Style.FillToken = GroundToken;
	Style.Weight = StrokeWeight;
	Frame->SetHairlineStyle(Style);
}

void UBRPanel::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyPanelStyle();
}

void UBRPanel::SetPanelHeight(float InHeight)
{
	if (!RootSizeBox)
	{
		return;
	}

	if (InHeight > 0.0f)
	{
		RootSizeBox->SetHeightOverride(InHeight);
	}
	else
	{
		RootSizeBox->ClearHeightOverride();
	}
}
