#include "UI/Components/BRMenuRow.h"

#include "Animation/WidgetAnimation.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "UI/Components/BRHairlineBorder.h"

const FName UBRMenuRow::PlateHoverParameterName(TEXT("Hover"));
const FName UBRMenuRow::PlatePressedParameterName(TEXT("Pressed"));

UBRMenuRow::UBRMenuRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	// The designer previews at the widget's own desired size instead of stretching it across a
	// 1280x720 canvas. See the header for why this cannot be a property write or a config
	// default. Editor-only data: absent from a packaged build entirely.
	DesignSizeMode = EDesignPreviewSizeMode::Desired;
#endif
}

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

	// A row that never calls SetSelectionText would otherwise render UMG's "Text Block"
	// placeholder on its right edge -- on every menu row on every screen, since only settings
	// rows carry a value. Routed through the setter so the empty state has ONE definition:
	// collapsed, not blank-but-occupying, and not a stale value from the previous row.
	SetSelectionText(FText::GetEmpty());

	ApplyRowType();
	SetRowAlignment(Alignment);
	ApplySelectedMark(GetSelected());

	// Force the idle treatment through the one code path rather than trusting WBP defaults.
	// No animation: nothing has been rendered yet, and a reverse play from time 0 is a no-op
	// that only exists to look like a bug in a profile capture.
	bIsInverted = true;
	ApplyInvertedState(false, /*bPlayAnimation*/ false);
}

void UBRMenuRow::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Designer preview for the two EditAnywhere axes. NativeOnInitialized does not run at design
	// time, so without this a RowType change in the details panel showed nothing until PIE.
	ApplyRowType();
	SetRowAlignment(Alignment);
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
	//
	// MAP VOTING IS THE EXCEPTION, and it is a measured one: there `Selection` is the second
	// line of `Text Stacked` (173x21 at x=10, directly under the label) rather than a value at
	// the right edge, so right-justifying it pushes the map name away from the label it
	// belongs to. It follows the label instead.
	if (Selection)
	{
		Selection->SetJustification(RowType == EBRMenuRowType::MapVoting
			? LabelJustify
			: ETextJustify::Right);
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

	// The material path first; it returns false unless this asset's plate carries one, and
	// then the original tint runs unchanged. Two ways to reach the same measured states.
	if (TextFrameFill && !ApplyPlateMaterialState(bInverted))
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

void UBRMenuRow::ApplyInversionToSubtree(UWidget* Root, const FSlateColor& InTextColor, bool bInverted)
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

bool UBRMenuRow::ApplyPlateMaterialState(bool bInverted)
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

	// Binary today, which makes this pixel-identical to the tint path. The reason it exists
	// is that a float can be eased and a token swap cannot -- see the header.
	PlateMaterial->SetScalarParameterValue(PlateHoverParameterName, bInverted ? 1.0f : 0.0f);

	// COMPONENT-SPECS Sec 2: pressed is the same plate as hover, so nothing drives this yet.
	// Written anyway so the parameter is never left at a stale value from a previous state.
	PlateMaterial->SetScalarParameterValue(PlatePressedParameterName, 0.0f);
	return true;
}

void UBRMenuRow::ApplySelectedMark(bool bSelected)
{
	// COMPONENT-SPECS Sec 2 Checkbox/Radio: Idle and Hover are an EMPTY box; only Active and
	// Active Hover carry the mark. HitTestInvisible rather than Visible -- the mark is
	// decoration inside a button and must never eat the click that toggles it.
	if (TypeCheckMark)
	{
		TypeCheckMark->SetVisibility(bSelected
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
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
	ApplySelectedMark(true);

	if (DisclosureAnim)
	{
		PlayAnimationForward(DisclosureAnim);
	}
}

void UBRMenuRow::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	ApplyInvertedState(IsHovered());
	ApplySelectedMark(false);

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
