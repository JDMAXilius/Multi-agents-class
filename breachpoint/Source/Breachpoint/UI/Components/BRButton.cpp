#include "UI/Components/BRButton.h"

#include "Animation/WidgetAnimation.h"
#include "Brushes/SlateColorBrush.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/Components/BRHairlineBorder.h"
#include "UI/Settings/BRSettingsDataObject.h"
#include "UI/Settings/BRSettingsKeyRemap.h"
#include "UI/Settings/BRSettingsValueObjects.h"
#include "UI/Styles/BRTextStyles.h"
#include "UI/Styles/BRUITokens.h"

// NOTE ON THE INCLUDE LIST -- it is longer than BRButton.h's on purpose and that is correct.
// The HEADER is the contract and it is native-only (CommonUI + UMG forward declarations +
// header-only tokens). A .cpp may reach for whatever it needs to implement that contract;
// nothing downstream inherits a translation unit's includes. `BRHairlineBorder.h` appears here
// and NOT in the header for exactly this reason: the border is a bind target the button drives,
// never part of its public shape.

// ============================================================================ Sec 1
// THE STYLES
// ============================================================================


namespace
{
	/** Every fill in the front end is a flat, sharp-cornered tint. No exceptions. */
	FSlateBrush Fill(const FLinearColor& Colour)
	{
		return FSlateColorBrush(Colour);
	}

	FSlateBrush NoFill()
	{
		return FSlateColorBrush(FLinearColor::Transparent);
	}
}

// -- UBRButtonStyleBase ----------------------------------------------------------------

UBRButtonStyleBase::UBRButtonStyleBase()
{
	bSingleMaterial = false;

	// Sane, invisible defaults. Concrete styles override what they care about; anything
	// left alone is a transparent rect rather than the engine's default white block.
	NormalBase = NoFill();
	NormalHovered = NoFill();
	NormalPressed = NoFill();
	SelectedBase = NoFill();
	SelectedHovered = NoFill();
	SelectedPressed = NoFill();
	Disabled = NoFill();

	ButtonPadding = FMargin(0.0f);
	CustomPadding = FMargin(0.0f);
	MinWidth = 0;
	MinHeight = 0;
	MaxWidth = 0;
	MaxHeight = 0;
}

// -- UBRButtonStyle_MenuRow ------------------------------------------------------------

UBRButtonStyle_MenuRow::UBRButtonStyle_MenuRow()
{
	// The inversion. Idle: nothing but type and a dim underline. Hover/selected: solid
	// white plate, black type. Pressed is the same plate -- the flip already reads as a
	// press; dimming it a second time just makes the row look broken.
	NormalBase = NoFill();
	NormalHovered = Fill(BR::Tokens::SelfWhite());
	NormalPressed = Fill(BR::Tokens::SelfWhite());
	SelectedBase = Fill(BR::Tokens::SelfWhite());
	SelectedHovered = Fill(BR::Tokens::SelfWhite());
	SelectedPressed = Fill(BR::Tokens::SelfWhite());
	Disabled = NoFill();

	NormalTextStyle = UBRTextStyle_MenuRow::StaticClass();
	NormalHoveredTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	SelectedTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	SelectedHoveredTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	DisabledTextStyle = UBRTextStyle_Caption::StaticClass();

	// Measured: row height 28, pitch 40 (REFERENCE-EXTRACTION.md Sec.3). The 12px of
	// pitch that is not row is spacing owned by the list, not padding owned by the row.
	MinHeight = BR::Tokens::MenuRowHeight;
	ButtonPadding = FMargin(16.0f, 0.0f, 16.0f, 0.0f);
}

// -- UBRButtonStyle_NavTab -------------------------------------------------------------

UBRButtonStyle_NavTab::UBRButtonStyle_NavTab()
{
	// Tabs do not take a plate. Hover is a whisper of Edge; the active state is the
	// outside border the widget draws at BR::Tokens::StrokeNavTabActive, and the
	// inactive state is the whole component at BR::Tokens::OpacityNavTabInactive.
	NormalBase = NoFill();
	NormalHovered = Fill(BR::Tokens::Edge(0.5f));
	NormalPressed = Fill(BR::Tokens::Edge());
	SelectedBase = NoFill();
	SelectedHovered = Fill(BR::Tokens::Edge(0.5f));
	SelectedPressed = Fill(BR::Tokens::Edge());
	Disabled = NoFill();

	NormalTextStyle = UBRTextStyle_Tab::StaticClass();
	NormalHoveredTextStyle = UBRTextStyle_Tab::StaticClass();
	SelectedTextStyle = UBRTextStyle_Tab::StaticClass();
	SelectedHoveredTextStyle = UBRTextStyle_Tab::StaticClass();
	DisabledTextStyle = UBRTextStyle_Caption::StaticClass();

	MinHeight = BR::Tokens::NavBarHeight;
	ButtonPadding = FMargin(12.0f, 0.0f, 12.0f, 0.0f);
}

// -- UBRButtonStyle_Highlight ----------------------------------------------------------

UBRButtonStyle_Highlight::UBRButtonStyle_Highlight()
{
	NormalBase = Fill(BR::Tokens::Amber());
	NormalHovered = Fill(BR::Tokens::SelfWhite());
	NormalPressed = Fill(BR::Tokens::SelfWhite());
	SelectedBase = Fill(BR::Tokens::SelfWhite());
	SelectedHovered = Fill(BR::Tokens::SelfWhite());
	SelectedPressed = Fill(BR::Tokens::SelfWhite());
	Disabled = Fill(BR::Tokens::Dead(0.35f));

	// Amber and white plates both take black ink -- neither is legible under white type.
	NormalTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	NormalHoveredTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	SelectedTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	SelectedHoveredTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	DisabledTextStyle = UBRTextStyle_Caption::StaticClass();

	MinHeight = BR::Tokens::MenuRowHeight;
	ButtonPadding = FMargin(20.0f, 0.0f, 20.0f, 0.0f);
}


// ============================================================================ Sec 2
// UBRButton
// ============================================================================


const FName UBRButton::PlateHoverParameterName(TEXT("Hover"));
const FName UBRButton::PlatePressedParameterName(TEXT("Pressed"));

UBRButton::UBRButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	// The designer previews at the widget's own desired size instead of stretching it across a
	// 1280x720 canvas. See the header for why this cannot be a property write or a config
	// default. Editor-only data: absent from a packaged build entirely.
	DesignSizeMode = EDesignPreviewSizeMode::Desired;
#endif
}

void UBRButton::NativeOnInitialized()
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

	// A row that never calls SetSelectionText would otherwise render UMG's "Text Block"
	// placeholder on its right edge -- on every menu row on every screen, since only settings
	// rows carry a value. Routed through the setter so the empty state has ONE definition:
	// collapsed, not blank-but-occupying, and not a stale value from the previous row.
	SetSelectionText(FText::GetEmpty());

	ApplyButtonType();
	SetButtonAlignment(Alignment);
	ApplySelectedMark(GetSelected());

	// Force the idle treatment through the one code path rather than trusting WBP defaults.
	// No animation: nothing has been rendered yet, and a reverse play from time 0 is a no-op
	// that only exists to look like a bug in a profile capture.
	bIsInverted = true;
	ApplyInvertedState(false, /*bPlayAnimation*/ false);
}

void UBRButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Designer preview for the two EditAnywhere axes. NativeOnInitialized does not run at design
	// time, so without this a ButtonType change in the details panel showed nothing until PIE.
	ApplyButtonType();
	SetButtonAlignment(Alignment);
	// Design time shows the IDLE variant, which is an empty box.
	ApplySelectedMark(false);

	// ...and the idle TREATMENT, for the same reason. Without this the designer draws
	// `TextFrameFill` with the engine's default WHITE brush -- an untinted UImage is a solid
	// white rectangle (BP70 D2) -- so every Menu Row asset opens as a white slab with its
	// border, label and type body hidden underneath. It is correct the moment it runs and
	// wrong every time anyone looks at it, which is the worst of the two.
	//
	// The bIsInverted flip is not redundant: ApplyInvertedState early-returns when the state
	// already matches, and the member defaults to false. NativeOnInitialized does the same
	// thing for the same reason.
	bIsInverted = true;
	ApplyInvertedState(false, /*bPlayAnimation*/ false);
}

void UBRButton::ApplyButtonType()
{
	if (RootSizeBox)
	{
		// COMPONENT-SPECS Sec 2 Type axis. Height is always driven; width is driven ONLY for
		// Icon Only, because a row otherwise fills its rail (349 on the front end, 536 in the
		// grid stack) and pinning it to 250 would break every wider list.
		float TypeHeight = RowHeight;
		switch (ButtonType)
		{
		case EBRButtonType::IconOnly:
			TypeHeight = IconOnlySize;
			break;

		case EBRButtonType::MapVoting:
			TypeHeight = MapVotingHeight;
			break;

		case EBRButtonType::Image:
			TypeHeight = ImageHeight;
			break;

		default:
			break;
		}

		RootSizeBox->SetHeightOverride(TypeHeight);

		if (ButtonType == EBRButtonType::IconOnly)
		{
			// 40 x 40 at BOTH design time and runtime: Icon Only is square by definition, and
			// a rail cannot stretch it into a row.
			RootSizeBox->SetWidthOverride(IconOnlySize);
		}
		else if (IsDesignTime())
		{
			// The measured component board is 250 wide. Only the designer gets it -- see
			// ComponentBoardWidth. Without this every asset previews at canvas width and
			// cannot be compared against Figma at all.
			RootSizeBox->SetWidthOverride(ComponentBoardWidth);
		}
		else
		{
			RootSizeBox->ClearWidthOverride();
		}
	}

	if (TypeSwitcher)
	{
		TypeSwitcher->SetActiveWidgetIndex(static_cast<int32>(ButtonType));
	}
}

void UBRButton::SetButtonType(EBRButtonType InButtonType)
{
	if (ButtonType == InButtonType)
	{
		return;
	}

	ButtonType = InButtonType;
	ApplyButtonType();
}

void UBRButton::SetButtonAlignment(EBRButtonAlignment InAlignment)
{
	Alignment = InAlignment;

	const ETextJustify::Type LabelJustify = (Alignment == EBRButtonAlignment::Center)
		? ETextJustify::Center
		: ETextJustify::Left;

	if (Label)
	{
		Label->SetJustification(LabelJustify);
	}

	// COMPONENT-SPECS Sec 2: `Selection` is right-aligned on both Alignment values -- it is the
	// value on a settings row, not part of the label block.
	//
	// MAP VOTING IS THE EXCEPTION, and it is a measured one: there `Selection` is the second
	// line of `Text Stacked` (173x21 at x=10, directly under the label) rather than a value at
	// the right edge, so right-justifying it pushes the map name away from the label it
	// belongs to. It follows the label instead.
	if (Selection)
	{
		Selection->SetJustification(ButtonType == EBRButtonType::MapVoting
			? LabelJustify
			: ETextJustify::Right);
	}
}

void UBRButton::SetLabelText(const FText& InText)
{
	if (Label)
	{
		Label->SetText(InText);
	}
}

void UBRButton::SetSelectionText(const FText& InText)
{
	if (!Selection)
	{
		return;
	}

	Selection->SetText(InText);

	// Honest empty state: an empty value renders as nothing, never as a stale previous value.
	Selection->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UBRButton::ApplyInvertedState(bool bInverted, bool bPlayAnimation)
{
	if (bIsInverted == bInverted)
	{
		return;
	}

	bIsInverted = bInverted;

	// MOTION-INTERACTION Sec 4.4: hover is folded into focus and the surviving hover value is a
	// 90 ms colour fade whose stated job is "stop a mouse sweep from strobing". `InvertAnim` is
	// where that lives; when it exists and will play, it owns the Hover scalar and C++ must not
	// pre-set it. See ApplyPlateMaterialState's header note.
	const bool bAnimationDrivesHover = (InvertAnim != nullptr) && bPlayAnimation;

	// The material path first; it returns false unless this asset's plate carries one, and
	// then the original tint runs unchanged. Two ways to reach the same measured states.
	if (TextFrameFill && !ApplyPlateMaterialState(bInverted, bAnimationDrivesHover))
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

	// COMPONENT-SPECS Sec 2, every Type's hover row: the body inverts with the label. Icon is
	// walked explicitly because it is a sibling of the body, not a child of it.
	if (Icon)
	{
		Icon->SetColorAndOpacity(TextColor.GetSpecifiedColor());
	}

	ApplyInversionToSubtree(TypeBody, TextColor, bInverted);

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

void UBRButton::ApplyInversionToSubtree(UWidget* Root, const FSlateColor& InTextColor, bool bInverted)
{
	// COMPONENT-SPECS Sec 2 Slider hover: the handle keeps a white ring and must not be tinted.
	// Checked before the type dispatch so the exemption covers the widget AND its children.
	if (!Root || Root == InversionExempt)
	{
		return;
	}

	if (UImage* AsImage = Cast<UImage>(Root))
	{
		// UImage takes a raw FLinearColor, unlike the text blocks' FSlateColor.
		AsImage->SetColorAndOpacity(InTextColor.GetSpecifiedColor());
	}
	else if (UCommonTextBlock* AsText = Cast<UCommonTextBlock>(Root))
	{
		AsText->SetColorAndOpacity(InTextColor);
	}
	else if (UBRHairlineBorder* AsLine = Cast<UBRHairlineBorder>(Root))
	{
		// A hairline resolves its own colour from tokens, so inverting it is a token swap, not
		// a tint. Both tokens move together: a checkbox square drawn half-dimmed on the
		// inverted plate reads as a rendering fault rather than as a state.
		FBRHairlineStyle LineStyle = AsLine->GetHairlineStyle();
		LineStyle.StrokeToken = bInverted ? InvertedTextToken : IdleTextToken;
		LineStyle.DimStrokeToken = LineStyle.StrokeToken;
		AsLine->SetHairlineStyle(LineStyle);
	}

	if (UPanelWidget* AsPanel = Cast<UPanelWidget>(Root))
	{
		const int32 NumChildren = AsPanel->GetChildrenCount();
		for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			ApplyInversionToSubtree(AsPanel->GetChildAt(ChildIndex), InTextColor, bInverted);
		}
	}
}

bool UBRButton::ApplyPlateMaterialState(bool bInverted, bool bAnimationDrivesHover)
{
	if (!TextFrameFill)
	{
		return false;
	}

	// A brushless UImage has no resource object at all, and a TEXTURE one is not a material.
	// Checked before GetDynamicMaterial() because that call CREATES a dynamic instance, and
	// creating one per row on an asset that will never use it is a real cost for nothing.
	if (!Cast<UMaterialInterface>(TextFrameFill->GetBrush().GetResourceObject()))
	{
		return false;
	}

	UMaterialInstanceDynamic* PlateMaterial = TextFrameFill->GetDynamicMaterial();
	if (!PlateMaterial)
	{
		return false;
	}

	// COLOUR IS STILL THE WIDGET'S, and that is the whole point of the material's shape.
	// FSlateBrush::TintColor multiplies the material output, so the tint carries the token
	// and the material only decides HOW MUCH of it shows. Set unconditionally: at idle the
	// alpha is 0, so a white tint is invisible rather than wrong.
	TextFrameFill->SetColorAndOpacity(BRUI::ResolveColorToken(InvertedFillToken));

	// Set the scalar ONLY when no animation is going to. With `InvertAnim` present this would
	// snap the plate to full and let the animation restart it from zero -- one flicker frame on
	// every hover, and the kind of defect that reads as "the material is broken".
	if (!bAnimationDrivesHover)
	{
		PlateMaterial->SetScalarParameterValue(PlateHoverParameterName, bInverted ? 1.0f : 0.0f);
	}

	// COMPONENT-SPECS Sec 2: pressed is the same plate as hover, so nothing drives this yet.
	// Written anyway so the parameter is never left at a stale value from a previous state.
	PlateMaterial->SetScalarParameterValue(PlatePressedParameterName, 0.0f);
	return true;
}

void UBRButton::ApplySelectedMark(bool bInSelected)
{
	// COMPONENT-SPECS Sec 2 Checkbox/Radio: Idle and Hover are an EMPTY box; only Active and
	// Active Hover carry the mark. HitTestInvisible rather than Visible -- the mark is
	// decoration inside a button and must never eat the click that toggles it.
	if (TypeCheckMark)
	{
		TypeCheckMark->SetVisibility(bInSelected
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UBRButton::NativeOnHovered()
{
	Super::NativeOnHovered();

	ApplyInvertedState(true);
}

void UBRButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	// A selected row stays inverted after the pointer leaves -- COMPONENT-SPECS Sec 2 Active is
	// "as Hover plus the disclosure glyph rotates", so selection owns the state too.
	ApplyInvertedState(GetSelected());
}

void UBRButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	ApplyInvertedState(true);
	ApplySelectedMark(true);

	if (DisclosureAnim)
	{
		PlayAnimationForward(DisclosureAnim);
	}
}

void UBRButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	ApplyInvertedState(IsHovered());
	ApplySelectedMark(false);

	if (DisclosureAnim)
	{
		PlayAnimationReverse(DisclosureAnim);
	}
}

void UBRButton::NativeOnEnabled()
{
	Super::NativeOnEnabled();

	SetRenderOpacity(1.0f);
}

void UBRButton::NativeOnDisabled()
{
	Super::NativeOnDisabled();

	// COMPONENT-SPECS Sec 2: Disabled dims every child to 0.5 and changes no geometry.
	SetRenderOpacity(DisabledOpacity);
}


// ============================================================================ Sec 3
// UBRSettingsRow
// ============================================================================


void UBRSettingsRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RefreshFromSetting();
}

void UBRSettingsRow::NativeDestruct()
{
	// A row torn down while still subscribed leaves the registry broadcasting into a widget the
	// viewport has already dropped. Unsubscribed here rather than relying on the delegate's weak
	// object check, because "it happens to be safe" is not the same as "nothing is listening".
	UnsubscribeFromSetting();

	Super::NativeDestruct();
}

void UBRSettingsRow::SetSetting(UBRSettingsDataObject* InSetting)
{
	if (Setting == InSetting)
	{
		return;
	}

	UnsubscribeFromSetting();

	Setting = InSetting;

	if (Setting)
	{
		Setting->OnSettingChanged.AddUObject(this, &UBRSettingsRow::HandleSettingChanged);
	}

	RefreshFromSetting();
}

void UBRSettingsRow::UnsubscribeFromSetting()
{
	if (Setting)
	{
		Setting->OnSettingChanged.RemoveAll(this);
	}
}

void UBRSettingsRow::HandleSettingChanged(UBRSettingsDataObject* Changed)
{
	// The delegate is per-object, so this only ever fires for our own setting. Refreshing
	// unconditionally rather than diffing: the row has four things on it and redrawing all of
	// them is cheaper than deciding which one moved.
	RefreshFromSetting();
}

EBRButtonType UBRSettingsRow::ResolveButtonType() const
{
	return ResolveButtonTypeFor(Setting);
}

EBRButtonType UBRSettingsRow::ResolveButtonTypeFor(const UBRSettingsDataObject* Setting)
{
	if (!Setting)
	{
		return EBRButtonType::Default;
	}

	// A collection IS the section header. It carries no value and must not look pressable.
	if (Setting->IsA<UBRSettingsCollection>())
	{
		return EBRButtonType::Default;
	}

	if (Setting->IsA<UBRSettingsValue_Scalar>())
	{
		return EBRButtonType::Slider;
	}

	if (const UBRSettingsValue_Discrete* Discrete = Cast<UBRSettingsValue_Discrete>(Setting))
	{
		// Two options is a toggle and reads as a checkbox; more is a list and reads as a
		// dropdown. This is the ONLY place the "a bool is a discrete with two options" decision
		// becomes visible to the player, which is the right place for it to surface.
		return Discrete->GetOptions().Num() == 2 ? EBRButtonType::Checkbox : EBRButtonType::DropDown;
	}

	// Key bindings render as a plain row: the value is the key name and the interaction is a
	// click, not a cycle.
	return EBRButtonType::Default;
}

void UBRSettingsRow::RefreshFromSetting()
{
	if (!Setting)
	{
		SetLabelText(FText::GetEmpty());
		SetSelectionText(FText::GetEmpty());
		return;
	}

	SetLabelText(Setting->GetDisplayName());
	SetButtonType(ResolveButtonType());

	const bool bIsHeader = Setting->IsA<UBRSettingsCollection>();

	// A header shows no value at all -- not the em dash. The dash means "not known yet", and a
	// section title is not a value that is pending.
	SetSelectionText(bIsHeader ? FText::GetEmpty() : Setting->GetDisplayValue());

	// CommonUI owns the disabled treatment; `UBRButton` already dims to 0.5 on it. A header is
	// non-interactive for the same call, which also keeps it out of the gamepad focus chain.
	SetIsInteractionEnabled(!bIsHeader && Setting->IsEditable());
	SetIsSelectable(!bIsHeader);
}

void UBRSettingsRow::NudgeValue(int32 Direction)
{
	if (!Setting || Direction == 0 || !Setting->IsEditable())
	{
		return;
	}

	if (UBRSettingsValue_Scalar* Scalar = Cast<UBRSettingsValue_Scalar>(Setting))
	{
		Scalar->StepBy(Direction);
		return;
	}

	if (UBRSettingsValue_Discrete* Discrete = Cast<UBRSettingsValue_Discrete>(Setting))
	{
		Discrete->CycleBy(Direction);
	}

	// Everything else -- headers, key bindings -- deliberately ignores a nudge. See the header.
}

void UBRSettingsRow::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (!Setting || !Setting->IsEditable())
	{
		return;
	}

	// A discrete row advances on click, which is what a player expects from clicking a toggle.
	// Everything else is handed up: the screen is what knows that a key binding opens a capture
	// modal, and a row that pushed its own screen would be a widget owning navigation.
	if (UBRSettingsValue_Discrete* Discrete = Cast<UBRSettingsValue_Discrete>(Setting))
	{
		Discrete->CycleBy(1);
		return;
	}

	OnSettingRowActivated.ExecuteIfBound(Setting);
}

// ============================================================================ Sec 4
// UBRHighlightButton -- accent fill, not white inversion. See the header.
// ============================================================================


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
