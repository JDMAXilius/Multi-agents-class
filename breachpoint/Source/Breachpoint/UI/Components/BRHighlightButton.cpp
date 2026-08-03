#include "UI/Components/BRHighlightButton.h"

#include "Animation/WidgetAnimation.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/WidgetSwitcher.h"
#include "UI/Components/BRHairlineBorder.h"

void UBRHighlightButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Border)
	{
		// COMPONENT-SPECS Sec 0 / Sec 2: 1px chrome, bottom line dimmed at rest. Set from C++ so
		// all 11 screens' buttons agree without 11 details panels.
		FBRHairlineStyle BorderStyle = Border->GetHairlineStyle();
		BorderStyle.Weight = EBRStrokeWeight::Chrome;
		Border->SetHairlineStyle(BorderStyle);
		Border->SetEdgeDimmed(EBRBorderEdge::Bottom, true);
	}

	ApplyButtonType();

	// Force the idle treatment through the one code path rather than trusting WBP defaults.
	// No animation: nothing has rendered yet, and a reverse play from time 0 is a no-op that
	// exists only to look like a bug in a profile capture.
	bIsInverted = true;
	ApplyInvertedState(false, /*bPlayAnimation*/ false);
}

EBRUIColorToken UBRHighlightButton::ResolveIdleFillToken() const
{
	switch (ButtonType)
	{
	case EBRHighlightButtonType::Event:
	case EBRHighlightButtonType::Premium:
		// ACCENT GAP -- see the class doc. COMPONENT-SPECS Sec 8 measures `#ff5c00` (event) and
		// the `Premium Yellow` variable `#ffc11c`. Neither is in EBRUIColorToken or BR::Tokens,
		// and both of those files belong to other lanes. Event and Premium therefore render as
		// Main today. When the styles lane adds the tokens this switch is the ONLY edit.
		return EBRUIColorToken::None;

	case EBRHighlightButtonType::Main:
	case EBRHighlightButtonType::Disabled:
	case EBRHighlightButtonType::Boring:
	case EBRHighlightButtonType::PhotoButton:
	default:
		// COMPONENT-SPECS Sec 1: idle has no fill at all. The plate exists only to be inverted.
		return EBRUIColorToken::None;
	}
}

void UBRHighlightButton::ApplyButtonType()
{
	if (TypeSwitcher)
	{
		TypeSwitcher->SetActiveWidgetIndex(static_cast<int32>(ButtonType));
	}

	// COMPONENT-SPECS Sec 2 status table: Disabled changes no geometry, only opacity -- and that
	// dim is CommonUI's `bApplyAlphaOnDisable` (0.5, set on the WBP CDO), not our arithmetic.
	// All we own is the fact that a Disabled-TYPE button is not interactive.
	SetIsInteractionEnabled(ButtonType != EBRHighlightButtonType::Disabled);

	if (Fill && !bIsInverted)
	{
		Fill->SetColorAndOpacity(BRUI::ResolveColorToken(ResolveIdleFillToken()));
	}
}

void UBRHighlightButton::SetButtonType(EBRHighlightButtonType InButtonType)
{
	if (ButtonType == InButtonType)
	{
		return;
	}

	ButtonType = InButtonType;
	ApplyButtonType();
}

void UBRHighlightButton::SetLabelText(const FText& InText)
{
	if (Label)
	{
		Label->SetText(InText);
	}
}

void UBRHighlightButton::ApplyInvertedState(bool bInverted, bool bPlayAnimation)
{
	if (bIsInverted == bInverted)
	{
		return;
	}

	bIsInverted = bInverted;

	// COMPONENT-SPECS Sec 1: "Idle -> Hover is an INVERSION, not a highlight." Three things, one
	// state. 1/3 -- the plate goes solid white.
	if (Fill)
	{
		const EBRUIColorToken FillToken = bInverted ? InvertedFillToken : ResolveIdleFillToken();
		Fill->SetColorAndOpacity(BRUI::ResolveColorToken(FillToken));
	}

	// 2/3 -- the label goes black.
	if (Label)
	{
		const FSlateColor TextColor(BRUI::ResolveColorToken(bInverted ? InvertedTextToken : IdleTextToken));
		Label->SetColorAndOpacity(TextColor);
	}

	// 3/3 -- the bottom line goes 0.3 -> 1.0.
	if (Border)
	{
		Border->SetEdgeDimmed(EBRBorderEdge::Bottom, !bInverted);
	}

	if (InvertAnim && bPlayAnimation)
	{
		// Timing lives in the timeline, authored at InversionDurationSeconds on EaseStandard.
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

void UBRHighlightButton::NativeOnHovered()
{
	Super::NativeOnHovered();

	ApplyInvertedState(true);
}

void UBRHighlightButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	// A selected button stays inverted after the pointer leaves -- selection owns the state too,
	// which is also what keeps the gamepad path (focus/select, no pointer) looking right.
	ApplyInvertedState(GetSelected());
}

void UBRHighlightButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	ApplyInvertedState(true);
}

void UBRHighlightButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	ApplyInvertedState(IsHovered());
}
