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

void UBNKillfeedEntry::SetEntry(const FBNKillfeedViewEntry& Entry)
{
	// The parts layout needs BOTH widgets and BOTH names. A kill with no killer ("X died") has no
	// parts to lay out, so it takes the composed line even in a WBP that binds all three.
	const bool bParts = KillerText && VictimText && !Entry.KillerText.IsEmpty() && !Entry.VictimText.IsEmpty();

	if (KillerText)
	{
		KillerText->SetText(Entry.KillerText);
		KillerText->SetVisibility(bParts ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (VictimText)
	{
		VictimText->SetText(Entry.VictimText);
		VictimText->SetVisibility(bParts ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LineText)
	{
		LineText->SetText(Entry.Line);
		LineText->SetVisibility(bParts ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (WeaponIcon)
	{
		const bool bHasIcon = !Entry.WeaponIcon.IsNull();
		if (bHasIcon && Entry.WeaponIcon != AppliedIcon)
		{
			WeaponIcon->SetBrushFromSoftTexture(Entry.WeaponIcon, /*bMatchSize=*/false);
			AppliedIcon = Entry.WeaponIcon;
		}
		// Collapsed, not Hidden: a feed line with no glyph must close the gap, not leave a hole
		// where the icon would be — the rows are a vertical list of variable-length lines.
		WeaponIcon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// The row, not the leaf: red is never spent here — a killfeed line is history, not threat.
	SetColorAndOpacity(Entry.bInvolvesSelf ? BNUIColors::Self : BNUIColors::InkDim);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBNKillfeedEntry::ClearEntry()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
