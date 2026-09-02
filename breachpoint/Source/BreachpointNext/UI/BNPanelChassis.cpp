#include "UI/BNPanelChassis.h"

#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/SizeBox.h"

void UBNPanelChassis::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		// C++ owns the width so four panels cannot drift to four widths. Height is per panel.
		RootSizeBox->SetWidthOverride(PanelWidth);
	}
}

void UBNPanelChassis::SetPanelHeight(float InHeight)
{
	if (RootSizeBox)
	{
		RootSizeBox->SetHeightOverride(InHeight);
	}
}

void UBNPanelChassis::SetCaretVisible(bool bVisible)
{
	if (Caret)
	{
		Caret->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
