#include "UI/Components/BRProgressBar.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "UI/Components/BRHairlineBorder.h"
#include "UI/Styles/BRUITokens.h"

#define LOCTEXT_NAMESPACE "Breachpoint"

UBRProgressBar::UBRProgressBar()
{
	// Honest empty state, ue5-ui-architecture Sec 7: dashes, not a confident "0".
	UnknownValueText = LOCTEXT("ProgressUnknown", "--");
}

void UBRProgressBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Frame)
	{
		// COMPONENT-SPECS Sec 0: 1px chrome is the default stroke. Pinned from C++ so the bar's
		// frame cannot fork per screen. The frame is never animated (MOTION-MEASURED headline).
		FBRHairlineStyle FrameStyle = Frame->GetHairlineStyle();
		FrameStyle.Weight = EBRStrokeWeight::Chrome;
		Frame->SetHairlineStyle(FrameStyle);
	}

	if (Track)
	{
		Track->SetColorAndOpacity(BRUI::ResolveColorToken(TrackGroundToken));
	}

	ApplyTreatment();

	// Construct in the unknown state rather than trusting a WBP default percent. A bar that
	// renders a confident value before any ViewModel has spoken is the join-in-progress bug.
	SetPercentUnknown();
}

FLinearColor UBRProgressBar::ResolveFillColor() const
{
	// ui-presentation Sec 4, the tier rule. This switch IS the difference between the lobby bar
	// and the HUD bar; there is no other place the two treatments part company on colour.
	if (Treatment == EBRProgressTreatment::FrontEndFlat)
	{
		// COMPONENT-SPECS Sec 0: the front end is white/black at varying alpha. The channel is
		// intentionally not read here -- a VISR channel in a menu would be the tier-rule
		// violation the skill names.
		return BRUI::ResolveColorToken(EBRUIColorToken::ChromeStroke);
	}

	// CombatVISR. UI-DESIGN-SYSTEM Sec 2 / ui-presentation Sec 3: one token, one meaning. The
	// hex lives in BR::Tokens and nowhere else -- see the contract gap in this packet's report
	// for why these do not come from BRUI::ResolveColorToken yet.
	switch (Channel)
	{
	case EBRProgressChannel::Shield:
		return BR::Tokens::Shield();

	case EBRProgressChannel::Health:
		return BR::Tokens::Health();

	case EBRProgressChannel::Amber:
		return BR::Tokens::Amber();

	case EBRProgressChannel::Enemy:
		return BR::Tokens::Enemy();

	case EBRProgressChannel::Neutral:
	default:
		return BR::Tokens::SelfWhite();
	}
}

void UBRProgressBar::ApplyTreatment()
{
	if (Fill)
	{
		Fill->SetFillColorAndOpacity(ResolveFillColor());
	}

	ApplyBarVisibility();
}

void UBRProgressBar::ApplyBarVisibility()
{
	// Two reasons a bar draws nothing, and both are honest rather than decorative:
	//   1. no value has arrived yet (join-in-progress),
	//   2. UI-DESIGN-SYSTEM Sec 2 / Halo Support: health under shields is HIDDEN UNTIL DAMAGED,
	//      and that is a HUD rule -- a front-end "match record" bar at 100% must still render,
	//      which is the second place the two treatments differ.
	const bool bHiddenUntilDamaged =
		Treatment == EBRProgressTreatment::CombatVISR
		&& Channel == EBRProgressChannel::Health
		&& bHasValue
		&& Percent >= FullPercent;

	const bool bVisible = bHasValue && !bHiddenUntilDamaged;

	if (Fill)
	{
		Fill->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (Track)
	{
		// Hidden, not Collapsed: the track reserves its slot so a bar appearing mid-fight does
		// not reflow the vitals block under the player's eyes.
		Track->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UBRProgressBar::SetPercent(float InPercent)
{
	Percent = FMath::Clamp(InPercent, 0.0f, FullPercent);
	bHasValue = true;

	if (Fill)
	{
		// Snaps. Smoothing is the ViewModel's job (ARCHITECTURE Sec 5.3) and the reference's own
		// fill timing is trailer choreography -- see the class doc.
		Fill->SetPercent(Percent);
	}

	ApplyBarVisibility();
}

void UBRProgressBar::SetPercentUnknown()
{
	Percent = 0.0f;
	bHasValue = false;

	if (Fill)
	{
		Fill->SetPercent(0.0f);
	}

	if (ValueText)
	{
		ValueText->SetText(UnknownValueText);
	}

	ApplyBarVisibility();
}

void UBRProgressBar::SetTreatment(EBRProgressTreatment InTreatment)
{
	if (Treatment == InTreatment)
	{
		return;
	}

	Treatment = InTreatment;
	ApplyTreatment();
}

void UBRProgressBar::SetChannel(EBRProgressChannel InChannel)
{
	if (Channel == InChannel)
	{
		return;
	}

	Channel = InChannel;
	ApplyTreatment();
}

void UBRProgressBar::SetValueText(const FText& InText)
{
	if (!ValueText)
	{
		return;
	}

	// An empty push reads as unknown, never as a stale previous number.
	ValueText->SetText(InText.IsEmpty() ? UnknownValueText : InText);
}

void UBRProgressBar::SetLabelText(const FText& InText)
{
	if (!LabelText)
	{
		return;
	}

	LabelText->SetText(InText);
	LabelText->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
