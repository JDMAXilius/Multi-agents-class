#include "UI/Components/BRTableRow.h"

#include "Animation/WidgetAnimation.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "UI/Components/BRHairlineBorder.h"

#define LOCTEXT_NAMESPACE "BRTableRow"

FText UBRTableRow::GetUnknownValueText()
{
	// Em dash as a universal-character escape so this file stays pure ASCII, same as
	// UBRProfileBar::GetUnknownValueText. GAP: two copies of one dash -- it wants to be
	// BRUI::GetUnknownValueText in BRComponentTokens.h, which is not this packet's file.
	return LOCTEXT("UnknownValue", "\u2014");
}

void UBRTableRow::SetColumnText(UCommonTextBlock* Column, const FText& InText)
{
	if (!Column)
	{
		return;
	}

	Column->SetText(InText.IsEmpty() ? GetUnknownValueText() : InText);
}

void UBRTableRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		// SCREEN-MANIFEST Sec 6.5: 660 x 28. Both browsers get the same shell from one place.
		RootSizeBox->SetWidthOverride(RowWidth);
		RootSizeBox->SetHeightOverride(RowHeight);
	}

	if (Border)
	{
		// SCREEN-MANIFEST Sec 6.5 is UBRHairlineBorder's default style exactly: top 1px @1.0,
		// bottom dimmed, side ticks dimmed. Only the tick length is stated per-shell.
		FBRHairlineStyle BorderStyle = Border->GetHairlineStyle();
		BorderStyle.SideTickLength = BorderSideTickLength;
		BorderStyle.Weight = EBRStrokeWeight::Chrome;
		Border->SetHairlineStyle(BorderStyle);
		Border->SetEdgeDimmed(EBRBorderEdge::Bottom, true);
	}

	ApplyRowType();
	SetSelectionMode(bSelectionMode);

	// Force the idle treatment through the one code path instead of trusting WBP defaults.
	// No animation: nothing has rendered yet, so a reverse play from time 0 is a no-op that
	// exists only to look like a bug in a profile capture.
	bIsInverted = true;
	ApplyInvertedState(false, /*bPlayAnimation*/ false);

	// A row constructed before its list item arrives shows dashes, not a plausible empty server.
	ClearToUnknown();
}

void UBRTableRow::ApplyRowType()
{
	// The Type axis is DATA on one class (see EBRTableRowType). It changes nothing about the
	// 660 x 28 shell; the columns differ only in what the producer pushes into them -- because
	// how the 660 divides is UNMEASURED (node 755:6805) and a guessed per-type layout would be
	// worse than none. The switcher below is the hook for the day that read happens.
	if (TypeSwitcher)
	{
		TypeSwitcher->SetActiveWidgetIndex(static_cast<int32>(RowType));
	}
}

void UBRTableRow::SetRowType(EBRTableRowType InRowType)
{
	if (RowType == InRowType)
	{
		return;
	}

	RowType = InRowType;
	ApplyRowType();
}

void UBRTableRow::SetSelectionMode(bool bInSelectionMode)
{
	bSelectionMode = bInSelectionMode;

	// CommonUI owns what "selected" means; this only says whether the row is a toggle at all.
	SetIsSelectable(bSelectionMode);

	if (SelectionMarker)
	{
		// Hidden, not Collapsed: collapsing the marker would shift every column to its right.
		SelectionMarker->SetVisibility(bSelectionMode
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Hidden);
	}
}

void UBRTableRow::SetRowItem(UBRTableRowItem* InItem)
{
	// Null is the normal case on the first frames after a browser query is issued, and after a
	// pooled entry is released. It is not an error and it is not a reason to keep stale text.
	if (!InItem || InItem->DataState == EBRUIDataState::Unknown)
	{
		ClearToUnknown();
		return;
	}

	SetRowType(InItem->RowType);

	SetColumnText(NameText, InItem->DisplayName);
	SetColumnText(AuthorText, InItem->AuthorName);
	SetColumnText(PlayersText, InItem->PlayerCount);

	if (RatingSlot)
	{
		// Negative rating means UNRATED. The column keeps its width and renders nothing rather
		// than showing zero stars, which would be a different and false statement.
		RatingSlot->SetVisibility(InItem->Rating >= 0.0f
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Hidden);
	}

	// GAP: UBRRating (85 x 17) does not exist yet, so nothing pushes InItem->Rating into a
	// widget. Visibility is all this class can honestly drive today.
}

void UBRTableRow::ClearToUnknown()
{
	SetColumnText(NameText, FText::GetEmpty());
	SetColumnText(AuthorText, FText::GetEmpty());
	SetColumnText(PlayersText, FText::GetEmpty());

	if (RatingSlot)
	{
		RatingSlot->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UBRTableRow::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// NOT `Super::` -- Super is UCommonButtonBase. IUserObjectListEntry's own implementation is
	// what routes the event on, and its header states the super call is expected.
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	// The cast is a checked one on purpose: a list fed the wrong item type must show the unknown
	// state, not the previous row's data. Silent stale rows are the bug this whole class avoids.
	SetRowItem(Cast<UBRTableRowItem>(ListItemObject));
}

void UBRTableRow::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);

	// The list owns selection state for its entries; the row only renders it. Deliberately does
	// NOT call SetIsSelected -- that would fight the list and re-enter this callback. Hover is
	// OR-ed in so deselecting a row the pointer is still over does not un-invert it.
	ApplyInvertedState(bIsSelected || IsHovered());
}

void UBRTableRow::NativeOnEntryReleased()
{
	IUserObjectListEntry::NativeOnEntryReleased();

	// Widget pooling (ue5-ui-architecture Sec 5): a released entry is reused for another row, so
	// it must not carry a single character of the old one into its next life.
	ClearToUnknown();
	ApplyInvertedState(false, /*bPlayAnimation*/ false);
}

void UBRTableRow::ApplyInvertedState(bool bInverted, bool bPlayAnimation)
{
	if (bIsInverted == bInverted)
	{
		return;
	}

	bIsInverted = bInverted;

	if (RowFill)
	{
		const EBRUIColorToken FillToken = bInverted ? InvertedFillToken : EBRUIColorToken::None;
		RowFill->SetColorAndOpacity(BRUI::ResolveColorToken(FillToken));
	}

	const FSlateColor TextColor(BRUI::ResolveColorToken(bInverted ? InvertedTextToken : IdleTextToken));

	for (UCommonTextBlock* Column : { NameText.Get(), AuthorText.Get(), PlayersText.Get() })
	{
		if (Column)
		{
			Column->SetColorAndOpacity(TextColor);
		}
	}

	// COMPONENT-SPECS Sec 2 hover: the bottom line goes 0.3 -> 1. One call, no lines redrawn.
	if (Border)
	{
		Border->SetEdgeDimmed(EBRBorderEdge::Bottom, !bInverted);
	}

	if (InvertAnim && bPlayAnimation)
	{
		if (bInverted)
		{
			PlayAnimationForward(InvertAnim);
		}
		else
		{
			PlayAnimationReverse(InvertAnim);
		}
	}
}

void UBRTableRow::NativeOnHovered()
{
	Super::NativeOnHovered();

	ApplyInvertedState(true);
}

void UBRTableRow::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	// A selected row stays inverted after the pointer leaves.
	ApplyInvertedState(GetSelected());
}

void UBRTableRow::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	ApplyInvertedState(true);
}

void UBRTableRow::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	ApplyInvertedState(IsHovered());
}

void UBRTableRow::NativeOnEnabled()
{
	Super::NativeOnEnabled();

	SetRenderOpacity(1.0f);
}

void UBRTableRow::NativeOnDisabled()
{
	Super::NativeOnDisabled();

	// COMPONENT-SPECS Sec 2 status table: Disabled dims to 0.5 and changes no geometry.
	SetRenderOpacity(DisabledOpacity);
}

void UBRSmallHeader::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		// SCREEN-MANIFEST Sec 1 / Sec 4: 1180 x 27, the 50px-margin family. The WIDTH is set here
		// so the deviation from the 69px grid lives in source next to the comment that explains
		// it; the ANCHOR (centre-stretch) is the WBP's, because anchors are layout.
		RootSizeBox->SetWidthOverride(HeaderWidth);
		RootSizeBox->SetHeightOverride(HeaderHeight);
	}

	ClearHeaderText();
}

void UBRSmallHeader::SetHeaderText(const FText& InText)
{
	if (HeaderText)
	{
		HeaderText->SetText(InText.IsEmpty() ? UBRTableRow::GetUnknownValueText() : InText);
	}
}

void UBRSmallHeader::ClearHeaderText()
{
	SetHeaderText(FText::GetEmpty());
}

#undef LOCTEXT_NAMESPACE
