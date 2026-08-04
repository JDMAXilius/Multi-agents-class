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
	// STILL `None` FOR EVERY TYPE, and now for a measured reason rather than a missing token.
	// The design file paints idle with THREE STACKED PAINTS -- `#000000@0.8`, a linear gradient
	// `#000000@0.3 -> accent` at `@0.8`, and `#000000@0.2` on top. `SetColorAndOpacity` applies a
	// single flat tint to one `UImage` and cannot express that stack, so returning any solid token
	// here would draw a flat plate the reference does not have. Closing this needs a gradient brush
	// or a material on `Fill`, which is a WBP/asset change, not an enum value.
	// Measurement: Content/UI/Components/Buttons/Assets/01-HighlightButton.md.
	return EBRUIColorToken::None;
}

EBRUIColorToken UBRHighlightButton::ResolveInvertedFillToken() const
{
	// Measured 4 Aug 2026 against Figma set `12:1194`, all 13 variants. The token hex matches the
	// file exactly in all three accent cases -- AccentHighlight #2EC3E5, AccentEvent #FF5C00,
	// AccentPremium #FFC11C -- so nothing is forked and no new token is owed.
	switch (ButtonType)
	{
	case EBRHighlightButtonType::Event:
		return EBRUIColorToken::AccentEvent;

	case EBRHighlightButtonType::Premium:
		return EBRUIColorToken::AccentPremium;

	case EBRHighlightButtonType::Boring:
		// The ONE type that really does invert to white. The old white-for-everything rule was
		// almost certainly generalised from this variant.
		return EBRUIColorToken::SurfaceInverted;

	case EBRHighlightButtonType::Disabled:
		// Measured hover is `#ffffff@0.3`. There is no 0.3-alpha white token (White20 is 0.2,
		// White50 is 0.5) and inventing one here would fork the palette in the one file nobody
		// greps. SurfaceInverted is returned and `ApplyButtonType`'s existing `DisabledOpacity`
		// (0.5) dims the whole widget, landing at white@0.5 against a measured 0.3.
		// KNOWN, SMALL, AND DELIBERATE -- filed rather than papered over.
		return EBRUIColorToken::SurfaceInverted;

	case EBRHighlightButtonType::Main:
	case EBRHighlightButtonType::PhotoButton:
	default:
		// Photo Button follows Main in the file: same accent, same bottom line.
		return EBRUIColorToken::AccentHighlight;
	}
}

void UBRHighlightButton::ApplyButtonType()
{
	if (TypeSwitcher)
	{
		TypeSwitcher->SetActiveWidgetIndex(static_cast<int32>(ButtonType));
	}

	// COMPONENT-SPECS Sec 2 status table: Disabled changes no geometry, only opacity. Both halves
	// are applied here -- see the class doc for why the dim is not left to CommonUI's
	// `bApplyAlphaOnDisable` (it no-ops before the Slate button exists, which is exactly when
	// this runs).
	const bool bTypeDisabled = (ButtonType == EBRHighlightButtonType::Disabled);
	SetIsInteractionEnabled(!bTypeDisabled);
	SetRenderOpacity(bTypeDisabled ? DisabledOpacity : 1.0f);

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
	// state. 1/3 -- the plate goes to THE TYPE'S ACCENT (white only for Boring).
	if (Fill)
	{
		const EBRUIColorToken FillToken = bInverted ? ResolveInvertedFillToken() : ResolveIdleFillToken();
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
