#include "UI/Components/BRMenuRow.h"

#include "Animation/WidgetAnimation.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "UI/Components/BRHairlineBorder.h"

void UBRMenuRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// COMPONENT-SPECS Sec 2: the border's side lines are ticks, not full edges, and the bottom
	// line starts dimmed. Set from C++ so all 26 screens' rows agree without 26 details panels.
	if (Border)
	{
		FBRHairlineStyle BorderStyle = Border->GetHairlineStyle();
		BorderStyle.SideTickLength = BorderSideTickLength;
		BorderStyle.Weight = EBRStrokeWeight::Chrome;
		Border->SetHairlineStyle(BorderStyle);
		Border->SetEdgeDimmed(EBRBorderEdge::Bottom, true);
	}

	if (BackgroundLine)
	{
		BackgroundLine->SetVisibility(ESlateVisibility::Collapsed);
	}

	ApplyRowType();
	SetRowAlignment(Alignment);

	// Force the idle treatment through the one code path rather than trusting WBP defaults.
	// No animation: nothing has been rendered yet, and a reverse play from time 0 is a no-op
	// that only exists to look like a bug in a profile capture.
	bIsInverted = true;
	ApplyInvertedState(false, /*bPlayAnimation*/ false);
}

void UBRMenuRow::ApplyRowType()
{
	if (RootSizeBox)
	{
		// COMPONENT-SPECS Sec 2 Type axis. Height is always driven; width is driven ONLY for
		// Icon Only, because a row otherwise fills its rail (349 on the front end, 536 in the
		// grid stack) and pinning it to 250 would break every wider list.
		float RowTypeHeight = RowHeight;
		switch (RowType)
		{
		case EBRMenuRowType::IconOnly:
			RowTypeHeight = IconOnlySize;
			break;

		case EBRMenuRowType::MapVoting:
			RowTypeHeight = MapVotingHeight;
			break;

		case EBRMenuRowType::Image:
			RowTypeHeight = ImageHeight;
			break;

		default:
			break;
		}

		RootSizeBox->SetHeightOverride(RowTypeHeight);

		if (RowType == EBRMenuRowType::IconOnly)
		{
			RootSizeBox->SetWidthOverride(IconOnlySize);
		}
		else
		{
			RootSizeBox->ClearWidthOverride();
		}
	}

	if (TypeSwitcher)
	{
		TypeSwitcher->SetActiveWidgetIndex(static_cast<int32>(RowType));
	}
}

void UBRMenuRow::SetRowType(EBRMenuRowType InRowType)
{
	if (RowType == InRowType)
	{
		return;
	}

	RowType = InRowType;
	ApplyRowType();
}

void UBRMenuRow::SetRowAlignment(EBRMenuRowAlignment InAlignment)
{
	Alignment = InAlignment;

	const ETextJustify::Type LabelJustify = (Alignment == EBRMenuRowAlignment::Center)
		? ETextJustify::Center
		: ETextJustify::Left;

	if (Label)
	{
		Label->SetJustification(LabelJustify);
	}

	// COMPONENT-SPECS Sec 2: `Selection` is right-aligned on both Alignment values -- it is the
	// value on a settings row, not part of the label block.
	if (Selection)
	{
		Selection->SetJustification(ETextJustify::Right);
	}
}

void UBRMenuRow::SetLabelText(const FText& InText)
{
	if (Label)
	{
		Label->SetText(InText);
	}
}

void UBRMenuRow::SetSelectionText(const FText& InText)
{
	if (!Selection)
	{
		return;
	}

	Selection->SetText(InText);

	// Honest empty state: an empty value renders as nothing, never as a stale previous value.
	Selection->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UBRMenuRow::ApplyInvertedState(bool bInverted, bool bPlayAnimation)
{
	if (bIsInverted == bInverted)
	{
		return;
	}

	bIsInverted = bInverted;

	if (TextFrameFill)
	{
		const EBRUIColorToken FillToken = bInverted ? InvertedFillToken : EBRUIColorToken::None;
		TextFrameFill->SetColorAndOpacity(BRUI::ResolveColorToken(FillToken));
	}

	const FSlateColor TextColor(BRUI::ResolveColorToken(bInverted ? InvertedTextToken : IdleTextToken));

	if (Label)
	{
		Label->SetColorAndOpacity(TextColor);
	}

	if (Selection)
	{
		Selection->SetColorAndOpacity(TextColor);
	}

	// COMPONENT-SPECS Sec 2 hover: Bottom Line opacity 0.3 -> 1.
	if (Border)
	{
		Border->SetEdgeDimmed(EBRBorderEdge::Bottom, !bInverted);
	}

	// COMPONENT-SPECS Sec 2 hover: "an extra Background Line (opacity 0.3) appears behind".
	if (BackgroundLine)
	{
		BackgroundLine->SetVisibility(bInverted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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

void UBRMenuRow::NativeOnHovered()
{
	Super::NativeOnHovered();

	ApplyInvertedState(true);
}

void UBRMenuRow::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	// A selected row stays inverted after the pointer leaves -- COMPONENT-SPECS Sec 2 Active is
	// "as Hover plus the disclosure glyph rotates", so selection owns the state too.
	ApplyInvertedState(GetSelected());
}

void UBRMenuRow::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	ApplyInvertedState(true);

	if (DisclosureAnim)
	{
		PlayAnimationForward(DisclosureAnim);
	}
}

void UBRMenuRow::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	ApplyInvertedState(IsHovered());

	if (DisclosureAnim)
	{
		PlayAnimationReverse(DisclosureAnim);
	}
}

void UBRMenuRow::NativeOnEnabled()
{
	Super::NativeOnEnabled();

	SetRenderOpacity(1.0f);
}

void UBRMenuRow::NativeOnDisabled()
{
	Super::NativeOnDisabled();

	// COMPONENT-SPECS Sec 2: Disabled dims every child to 0.5 and changes no geometry.
	SetRenderOpacity(DisabledOpacity);
}
