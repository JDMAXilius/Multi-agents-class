#include "UI/BNKillfeedEntry.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/BNUITypes.h"

void UBNKillfeedEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBNKillfeedEntry::SetEntry(const FText& Line, bool bInvolvesSelf, const TSoftObjectPtr<UTexture2D>& WeaponIconAsset)
{
	if (LineText)
	{
		LineText->SetText(Line);
	}
	if (WeaponIcon)
	{
		const bool bHasIcon = !WeaponIconAsset.IsNull();
		if (bHasIcon && WeaponIconAsset != AppliedIcon)
		{
			WeaponIcon->SetBrushFromSoftTexture(WeaponIconAsset, /*bMatchSize=*/false);
			AppliedIcon = WeaponIconAsset;
		}
		// Collapsed, not Hidden: a feed line with no glyph must close the gap, not leave a hole
		// where the icon would be — the rows are a vertical list of variable-length lines.
		WeaponIcon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// The row, not the leaf: red is never spent here — a killfeed line is history, not threat.
	SetColorAndOpacity(bInvolvesSelf ? BNUIColors::Self : BNUIColors::InkDim);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBNKillfeedEntry::ClearEntry()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
