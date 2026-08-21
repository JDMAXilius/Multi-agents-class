#include "UI/BNKillfeedEntry.h"
#include "Components/TextBlock.h"
#include "UI/BNUITypes.h"

void UBNKillfeedEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBNKillfeedEntry::SetEntry(const FText& Line, bool bInvolvesSelf)
{
	if (LineText)
	{
		LineText->SetText(Line);
	}
	// The row, not the leaf: red is never spent here — a killfeed line is history, not threat.
	SetColorAndOpacity(bInvolvesSelf ? BNUIColors::Self : BNUIColors::InkDim);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBNKillfeedEntry::ClearEntry()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
