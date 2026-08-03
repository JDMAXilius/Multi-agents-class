#include "UI/Components/BRRosterPanel.h"

#include "Breachpoint.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "UI/Components/BRHairlineBorder.h"

#define LOCTEXT_NAMESPACE "BRRosterPanel"

namespace BRRosterPanelInternal
{
	/** One owner for the unknown dash: `BRUI::UnknownValueText` (CPP-AUDIT fork collapse). */
	FText UnknownValue()
	{
		return BRUI::UnknownValueText();
	}

	/** Show or collapse a widget by whether it has anything to say. Collapsed, never Hidden: a
	 *  hidden icon still reserves its 26px and leaves a hole where art should be. */
	void SetShown(UWidget* Widget, bool bShown)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	/**
	 * Law 3: soft only. `SetBrushFromSoftTexture` streams it in; a null soft ptr collapses the
	 * slot rather than drawing an empty white box that reads as "this player has no emblem".
	 */
	void ApplySoftIcon(UImage* Image, const TSoftObjectPtr<UTexture2D>& SoftTexture)
	{
		if (!Image)
		{
			return;
		}

		if (SoftTexture.IsNull())
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		Image->SetBrushFromSoftTexture(SoftTexture, /*bMatchSize*/ false);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

// ===============================================================================================
// UBRRosterHeader
// ===============================================================================================

void UBRRosterHeader::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		RootSizeBox->SetHeightOverride(HeaderHeight);
	}

	// White plate, dark text -- COMPONENT-SPECS Sec 1's inversion, applied permanently. Both come
	// from tokens so the header cannot drift away from a hovered menu row's white.
	if (Ground)
	{
		Ground->SetColorAndOpacity(BRUI::ResolveColorToken(GroundToken));
	}

	const FSlateColor TextColor(BRUI::ResolveColorToken(TextToken));

	if (Label)
	{
		Label->SetColorAndOpacity(TextColor);
	}

	if (Count)
	{
		Count->SetColorAndOpacity(TextColor);
	}

	ClearToUnknown();
}

void UBRRosterHeader::SetHeader(const FText& InLabel, int32 InMemberCount, int32 InCapacity)
{
	if (Label)
	{
		Label->SetText(InLabel.IsEmpty() ? BRRosterPanelInternal::UnknownValue() : InLabel);
	}

	if (!Count)
	{
		return;
	}

	// A negative on either side means "not known yet". Rendering `0/4` in that case would be a
	// confident lie about an empty fireteam -- `ue5-ui-architecture` Sec 7 calls that a finding,
	// not a cosmetic nit.
	if (InMemberCount < 0 || InCapacity < 0)
	{
		Count->SetText(BRRosterPanelInternal::UnknownValue());
		return;
	}

	Count->SetText(FText::Format(
		LOCTEXT("HeaderCountFormat", "{0}/{1}"),
		FText::AsNumber(InMemberCount),
		FText::AsNumber(InCapacity)));
}

void UBRRosterHeader::ClearToUnknown()
{
	SetHeader(FText::GetEmpty(), INDEX_NONE, INDEX_NONE);
}

// ===============================================================================================
// UBRRosterRow
// ===============================================================================================

FText UBRRosterRow::GetUnknownValueText()
{
	return BRRosterPanelInternal::UnknownValue();
}

EBRRosterTextTone UBRRosterRow::ComputeTextTone(const FLinearColor& InFillColor)
{
	// COMPONENT-SPECS Sec 4: the gamertag flips against the TEAM FILL's luminance. The fill is
	// composited over a dark panel ground (`PanelGround40` over a dark gradient), so a fill at low
	// alpha is mostly ground and must still read as "dark" -- hence the alpha scaling. Without it,
	// a bright team colour at alpha 0.1 would flip the text to black on a near-black row.
	const float EffectiveLuminance = InFillColor.GetLuminance() * InFillColor.A;

	return (EffectiveLuminance > TextToneLuminanceThreshold)
		? EBRRosterTextTone::OnLight
		: EBRRosterTextTone::OnDark;
}

void UBRRosterRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		// Height only. Width is the container's business -- see the 349/310 note on the panel.
		RootSizeBox->SetHeightOverride(RowHeight);
	}

	if (Border)
	{
		// COMPONENT-SPECS Sec 4: the row border is the four-line set with the WHOLE FRAME at
		// opacity 0.3, not a per-edge dim mask. So every edge draws at the full chrome token and
		// the frame's render opacity carries the 0.3 -- exactly what the Figma layer opacity does.
		FBRHairlineStyle BorderStyle = Border->GetHairlineStyle();
		BorderStyle.Edges = static_cast<int32>(EBRBorderEdge::Top)
			| static_cast<int32>(EBRBorderEdge::Bottom)
			| static_cast<int32>(EBRBorderEdge::Left)
			| static_cast<int32>(EBRBorderEdge::Right);
		BorderStyle.DimmedEdges = static_cast<int32>(EBRBorderEdge::None);
		BorderStyle.Weight = EBRStrokeWeight::Chrome;
		BorderStyle.SideTickLength = 0.0f; // 0 = run the full height: a row border is a closed box.
		Border->SetHairlineStyle(BorderStyle);
		Border->SetRenderOpacity(BorderOpacity);
	}

	if (RankFrame)
	{
		// COMPONENT-SPECS Sec 4: `#ffffff@0.5`. Token for the colour, measured opacity for the
		// alpha; no white@0.5 token exists yet (contract_gap, styles lane).
		RankFrame->SetColorAndOpacity(BRUI::ResolveColorToken(RankFrameFillToken));
		RankFrame->SetRenderOpacity(RankFrameFillOpacity);
	}

	ApplyExternalIconLayout();

	// Explicit unknown before anything is pushed. A roster row that renders a blank gamertag on the
	// first frames after a mid-match join is telling the player a stranger has no name.
	ClearToUnknown();
}

void UBRRosterRow::ApplyExternalIconLayout()
{
	// THE -5 (COMPONENT-SPECS Sec 4 `External Icons` auto-layout gap).
	//
	// `ExternalIcons` is a UOverlay and NOT a UHorizontalBox: an HBox lays children end to end and
	// has no way to make two children share pixels, and negative slot padding on an HBox shrinks
	// the slot instead of walking the child backwards over its sibling. In an Overlay both children
	// are anchored to the same origin, so a left padding of
	//
	//     PartyLeaderIconSize + ExternalIconsGap  ==  30 + (-5)  ==  25
	//
	// puts the 10px current-player icon 25 from the left, i.e. overlapping the last 5px of the 30px
	// party-leader frame. The overlap is deliberate and is driven from C++ so the sign cannot be
	// lost in a details panel. `ExternalIconsGap` is UNVERIFIED (figma_geometry.json
	// `_pending_measurement.UBRRosterRow._verify` item 1) -- when it is re-measured, this one
	// constant is the only thing that changes.
	if (!ExternalIcons || !CurrentPlayerIcon)
	{
		return;
	}

	if (UOverlaySlot* IconSlot = Cast<UOverlaySlot>(CurrentPlayerIcon->Slot))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Left);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(CurrentPlayerIconOffsetX, 0.0f, 0.0f, 0.0f));
	}

	if (UOverlaySlot* LeaderSlot = PartyLeaderIcon ? Cast<UOverlaySlot>(PartyLeaderIcon->Slot) : nullptr)
	{
		LeaderSlot->SetHorizontalAlignment(HAlign_Left);
		LeaderSlot->SetVerticalAlignment(VAlign_Center);
		LeaderSlot->SetPadding(FMargin(0.0f));
	}
}

void UBRRosterRow::SetMember(const FBRRosterMemberView& InMember)
{
	Member = InMember;
	DataState = EBRUIDataState::Live;
	ApplyMember();
}

void UBRRosterRow::ClearToUnknown()
{
	Member = FBRRosterMemberView();
	DataState = EBRUIDataState::Unknown;
	ApplyMember();
}

void UBRRosterRow::ApplyMember()
{
	const bool bKnown = (DataState != EBRUIDataState::Unknown);

	// The tone is COMPUTED from whatever fill was pushed -- never a stored pair of colours that
	// somebody has to remember to flip alongside the fill.
	TextTone = ComputeTextTone(Member.TeamFillColor);

	if (TeamFill)
	{
		TeamFill->SetColorAndOpacity(Member.TeamFillColor);
	}

	if (Gamertag)
	{
		// Empty is the dash, always. A blank line reads as "this seat is empty", which is a
		// different and possibly false statement from "we have not been told the name yet".
		Gamertag->SetText(
			(bKnown && !Member.Gamertag.IsEmpty()) ? Member.Gamertag : GetUnknownValueText());
	}

	// The empty soft ptr is spelled out rather than `nullptr`: the two arms of a ternary have to
	// agree on a type, and `TSoftObjectPtr<UTexture2D>` vs `nullptr_t` does not deduce.
	const TSoftObjectPtr<UTexture2D> NoIcon;
	BRRosterPanelInternal::ApplySoftIcon(Emblem, bKnown ? Member.Emblem : NoIcon);
	BRRosterPanelInternal::ApplySoftIcon(RankInsignia, bKnown ? Member.RankInsignia : NoIcon);

	// The rank plate only exists to sit behind an insignia; with no insignia it is a white smear.
	BRRosterPanelInternal::SetShown(RankFrame, bKnown && !Member.RankInsignia.IsNull());

	if (MicSwitcher)
	{
		const EBRRosterMicState MicState = bKnown ? Member.MicState : EBRRosterMicState::Unknown;
		MicSwitcher->SetActiveWidgetIndex(static_cast<int32>(MicState));

		// Unknown and None both draw nothing: we do not claim a player is un-muted before voice
		// state has arrived.
		BRRosterPanelInternal::SetShown(MicSwitcher,
			MicState != EBRRosterMicState::Unknown && MicState != EBRRosterMicState::None);
	}

	BRRosterPanelInternal::SetShown(PartyLeaderIcon, bKnown && Member.bIsPartyLeader);
	BRRosterPanelInternal::SetShown(CurrentPlayerIcon, bKnown && Member.bIsLocalPlayer);
	BRRosterPanelInternal::SetShown(ExternalIcons,
		bKnown && (Member.bIsPartyLeader || Member.bIsLocalPlayer));

	ApplyTextTone();
}

void UBRRosterRow::ApplyTextTone()
{
	const EBRUIColorToken ToneToken = (TextTone == EBRRosterTextTone::OnLight)
		? ToneOnLightToken
		: ToneOnDarkToken;

	const FLinearColor ToneColor = BRUI::ResolveColorToken(ToneToken);

	if (Gamertag)
	{
		Gamertag->SetColorAndOpacity(FSlateColor(ToneColor));
	}

	// COMPONENT-SPECS Sec 4 ships the mic glyph as `... x White / Black`. That is the same flip,
	// so it is a TINT on the one active glyph rather than six art variants. If the active child is
	// not a UImage the WBP has chosen its own treatment and we leave it alone.
	if (MicSwitcher)
	{
		if (UImage* ActiveGlyph = Cast<UImage>(MicSwitcher->GetActiveWidget()))
		{
			ActiveGlyph->SetColorAndOpacity(ToneColor);
		}
	}
}

// ===============================================================================================
// UBRRosterPanel
// ===============================================================================================

UBRRosterPanel::UBRRosterPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RowPool(*this)
{
}

FText UBRRosterPanel::GetUnknownValueText()
{
	return BRRosterPanelInternal::UnknownValue();
}

void UBRRosterPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RowPool.SetWorld(GetWorld());
	RowPool.SetDefaultPlayerController(GetOwningPlayer());

	if (!RowWidgetClass.IsNull())
	{
		ResolvedRowClass = RowWidgetClass.LoadSynchronous();
	}

	ApplyPanelGeometry();

	// Explicit UNKNOWN before anything is pushed. There is no ViewModel to ask and this widget is
	// forbidden from asking anything else, so "unknown" is the only honest opening state.
	ClearToUnknown();
}

void UBRRosterPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	RowPool.ReleaseAllSlateResources();

	Super::ReleaseSlateResources(bReleaseChildren);
}

void UBRRosterPanel::ApplyPanelGeometry()
{
	if (RootSizeBox)
	{
		// 349 x 273 measured (node 527:3515). Width is the ONE knob for the 349/310 conflict; the
		// panel is anchored RIGHT by its canvas slot in the WBP, which is what keeps it 69 from the
		// right edge on ultrawide with no extra code.
		RootSizeBox->SetWidthOverride(PanelWidth);
		RootSizeBox->SetHeightOverride(PanelHeight);
	}

	if (GradientFill)
	{
		// COMPONENT-SPECS Sec 6: "fill linear gradient @0.5". The gradient itself is Tier 4 art;
		// C++ owns only the measured opacity.
		GradientFill->SetRenderOpacity(GradientOpacity);
	}

	if (Ground)
	{
		Ground->SetColorAndOpacity(BRUI::ResolveColorToken(GroundToken));

		// COMPONENT-SPECS Sec 6: "Background boolean `#000000@0.4` inset 3". Pushed into the slot
		// from here so the 3 is not retyped in the WBP. Only applies when `Ground` sits in an
		// Overlay, which is the intended authoring; otherwise the WBP owns its own inset.
		if (UOverlaySlot* GroundSlot = Cast<UOverlaySlot>(Ground->Slot))
		{
			GroundSlot->SetPadding(FMargin(BackgroundInset));
		}
	}

	if (PanelBorder)
	{
		// COMPONENT-SPECS Sec 6: `#ffffff@0.2` 1px, all four edges, full length -- a closed box,
		// unlike a menu row's ticks. Full-white token + measured render opacity, because there is
		// no white@0.2 token (`ChromeStrokeDim` is 0.3). contract_gap: styles lane.
		//
		// The measurement also says stroke align INSIDE. `FBRHairlineStyle` has no alignment field
		// and draws CENTER, a 0.5px difference on a 1px stroke. NOT worked around here --
		// contract_gap filed against `BRHairlineBorder.h`, which is another packet's owner path.
		FBRHairlineStyle BorderStyle = PanelBorder->GetHairlineStyle();
		BorderStyle.Edges = static_cast<int32>(EBRBorderEdge::Top)
			| static_cast<int32>(EBRBorderEdge::Bottom)
			| static_cast<int32>(EBRBorderEdge::Left)
			| static_cast<int32>(EBRBorderEdge::Right);
		BorderStyle.DimmedEdges = static_cast<int32>(EBRBorderEdge::None);
		BorderStyle.StrokeToken = StrokeToken;
		BorderStyle.Weight = EBRStrokeWeight::Chrome;
		BorderStyle.SideTickLength = 0.0f;
		PanelBorder->SetHairlineStyle(BorderStyle);
		PanelBorder->SetRenderOpacity(StrokeOpacity);
	}
}

void UBRRosterPanel::NativePreConstruct()
{
	Super::NativePreConstruct();

	// CPP-AUDIT P3: `PanelWidth` is the one documented designer knob on this class (the 349/310
	// conflict) and it only applied from NativeOnInitialized — which never runs at design time,
	// so the designer typed 310 and saw nothing. PreConstruct runs in the UMG designer preview.
	ApplyPanelGeometry();
}

void UBRRosterPanel::SetHeaderText(const FText& InLabel, int32 InCapacity)
{
	HeaderLabel = InLabel;
	HeaderCapacity = InCapacity;

	if (!Header)
	{
		return;
	}

	// Count is only meaningful once a feed has actually spoken. Before that it is the dash.
	const int32 MemberCount = (DataState == EBRUIDataState::Unknown) ? INDEX_NONE : Members.Num();
	Header->SetHeader(HeaderLabel, MemberCount, HeaderCapacity);
}

void UBRRosterPanel::SetMembers(const TArray<FBRRosterMemberView>& InMembers)
{
	Members = InMembers;
	DataState = EBRUIDataState::Live;

	RebuildRows();
	SetHeaderText(HeaderLabel, HeaderCapacity);
	ApplyEmptyState();
}

void UBRRosterPanel::ClearToUnknown()
{
	Members.Reset();
	DataState = EBRUIDataState::Unknown;

	RebuildRows();

	if (Header)
	{
		Header->ClearToUnknown();
	}

	ApplyEmptyState();
}

void UBRRosterPanel::ApplyEmptyState()
{
	if (!EmptyStateLabel)
	{
		return;
	}

	if (DataState == EBRUIDataState::Unknown)
	{
		// "We have not been told." Distinct from "there is nobody here" on purpose.
		EmptyStateLabel->SetText(GetUnknownValueText());
		EmptyStateLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	if (Members.Num() == 0)
	{
		EmptyStateLabel->SetText(LOCTEXT("RosterEmpty", "NO PLAYERS"));
		EmptyStateLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	EmptyStateLabel->SetVisibility(ESlateVisibility::Collapsed);
}

void UBRRosterPanel::RebuildRows()
{
	if (!RowContainer)
	{
		return;
	}

	RowContainer->ClearChildren();
	RowPool.ReleaseAll(/*bReleaseSlate*/ false);

	if (!ResolvedRowClass)
	{
		// No row class configured is a WBP authoring error, not a runtime state. Say so once
		// rather than rendering an empty panel that looks like an empty roster.
		if (DataState != EBRUIDataState::Unknown)
		{
			UE_LOG(Logbreachpoint, Warning,
				TEXT("UBRRosterPanel '%s' has no RowWidgetClass; %d roster members cannot be shown."),
				*GetName(), Members.Num());
		}
		return;
	}

	const int32 NumToShow = FMath::Min(Members.Num(), MaxVisibleRows);

	for (int32 Index = 0; Index < NumToShow; ++Index)
	{
		UBRRosterRow* Row = RowPool.GetOrCreateInstance<UBRRosterRow>(ResolvedRowClass);
		if (!Row)
		{
			break;
		}

		Row->SetMember(Members[Index]);

		if (UVerticalBoxSlot* RowSlot = RowContainer->AddChildToVerticalBox(Row))
		{
			// Measured pitch 35 on a 30-tall row. The gap is expressed as bottom padding on every
			// row but the last, so the column's total height is exactly N*30 + (N-1)*5 and the
			// panel's 273 stays honest.
			const float BottomPadding = (Index == NumToShow - 1) ? 0.0f : UBRRosterRow::RowGap;
			RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	// `ue5-ui-architecture` Sec 5: a silent cap reads as "we covered everything" when we did not.
	if (Members.Num() > NumToShow)
	{
		UE_LOG(Logbreachpoint, Warning,
			TEXT("UBRRosterPanel '%s' dropped %d roster member(s): %d pushed, %d fit in the "
				 "measured %.0fx%.0f panel."),
			*GetName(), Members.Num() - NumToShow, Members.Num(), NumToShow, PanelWidth, PanelHeight);
	}
}

#undef LOCTEXT_NAMESPACE
