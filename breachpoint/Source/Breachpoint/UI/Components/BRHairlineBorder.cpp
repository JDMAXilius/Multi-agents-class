#include "UI/Components/BRHairlineBorder.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/WidgetStyle.h"

#define LOCTEXT_NAMESPACE "BRHairlineBorder"

namespace BRHairlineBorderInternal
{
	FORCEINLINE bool HasEdge(int32 Mask, EBRBorderEdge Edge)
	{
		return (Mask & static_cast<int32>(Edge)) != 0;
	}

	FLinearColor ResolveEdgeColor(const FBRHairlineStyle& Style, EBRBorderEdge Edge)
	{
		if (!HasEdge(Style.Edges, Edge))
		{
			return FLinearColor::Transparent;
		}

		const EBRUIColorToken Token = HasEdge(Style.DimmedEdges, Edge) ? Style.DimStrokeToken : Style.StrokeToken;
		return BRUI::ResolveColorToken(Token);
	}
}

void SBRHairlineBorder::Construct(const FArguments& InArgs)
{
	Params = InArgs._Params;
	SetCanTick(false);
}

void SBRHairlineBorder::SetParams(const FBRHairlineRenderParams& InParams)
{
	Params = InParams;
	Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

FVector2D SBRHairlineBorder::ComputeDesiredSize(float /*LayoutScaleMultiplier*/) const
{
	// A hairline's natural size is its own weight on both axes. In a Fill-aligned box slot the
	// major axis stretches and the minor axis stays 0.5 / 1 / 2 px -- which is exactly what a
	// rule and a border both want, with no size typed into the WBP.
	return FVector2D(Params.Thickness, Params.Thickness);
}

int32 SBRHairlineBorder::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());

	if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
	{
		return LayerId;
	}

	int32 CurrentLayer = LayerId;

	if (Params.FillColor.A > 0.0f)
	{
		// A colour brush needs no texture and no asset -- COMPONENT-SPECS' plates are flat
		// fills. Engine-owned, not a function-local static: a static FSlateBrush's destruction
		// order against the Slate renderer's resource map is the classic shutdown assert, and
		// this is the hottest paint path in the front end (CPP-AUDIT P3).
		const FSlateBrush* FillBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			CurrentLayer,
			AllottedGeometry.ToPaintGeometry(),
			FillBrush,
			DrawEffects,
			Params.FillColor * Tint);

		++CurrentLayer;
	}

	const float Thickness = FMath::Max(Params.Thickness, UE_KINDA_SMALL_NUMBER);
	const float HalfThickness = Thickness * 0.5f;

	// Sub-pixel weights need antialiasing to survive; whole-pixel chrome is crisper without it.
	const bool bAntialias = Thickness < BRUI::ChromeWeightPx;

	const float TickLength = (Params.SideTickLength > 0.0f)
		? FMath::Min(Params.SideTickLength, LocalSize.Y)
		: LocalSize.Y;
	const float TickTop = (LocalSize.Y - TickLength) * 0.5f;
	const float TickBottom = TickTop + TickLength;

	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();

	TArray<FVector2f> Points;
	Points.SetNumUninitialized(2);

	auto DrawEdge = [&](const FLinearColor& Color, const FVector2f& Start, const FVector2f& End)
	{
		if (Color.A <= 0.0f)
		{
			return;
		}

		Points[0] = Start;
		Points[1] = End;

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			CurrentLayer,
			PaintGeometry,
			Points,
			DrawEffects,
			Color * Tint,
			bAntialias,
			Thickness);
	};

	// COMPONENT-SPECS Sec 2: stroke alignment is CENTER, so each line sits half a weight inside
	// its own edge. That keeps a 2px emphasis border inside the component box instead of
	// bleeding one pixel over the neighbour.
	DrawEdge(Params.TopColor, FVector2f(0.0f, HalfThickness), FVector2f(LocalSize.X, HalfThickness));
	DrawEdge(Params.BottomColor, FVector2f(0.0f, LocalSize.Y - HalfThickness), FVector2f(LocalSize.X, LocalSize.Y - HalfThickness));
	DrawEdge(Params.LeftColor, FVector2f(HalfThickness, TickTop), FVector2f(HalfThickness, TickBottom));
	DrawEdge(Params.RightColor, FVector2f(LocalSize.X - HalfThickness, TickTop), FVector2f(LocalSize.X - HalfThickness, TickBottom));

	return CurrentLayer;
}

UBRHairlineBorder::UBRHairlineBorder()
{
	// Decoration must never eat a click meant for the control it decorates.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FBRHairlineRenderParams UBRHairlineBorder::BuildRenderParams() const
{
	using namespace BRHairlineBorderInternal;

	FBRHairlineRenderParams Result;
	Result.FillColor = BRUI::ResolveColorToken(HairlineStyle.FillToken);
	Result.TopColor = ResolveEdgeColor(HairlineStyle, EBRBorderEdge::Top);
	Result.BottomColor = ResolveEdgeColor(HairlineStyle, EBRBorderEdge::Bottom);
	Result.LeftColor = ResolveEdgeColor(HairlineStyle, EBRBorderEdge::Left);
	Result.RightColor = ResolveEdgeColor(HairlineStyle, EBRBorderEdge::Right);
	Result.Thickness = BRUI::ResolveStrokeWeightPx(HairlineStyle.Weight);
	Result.SideTickLength = HairlineStyle.SideTickLength;

	return Result;
}

TSharedRef<SWidget> UBRHairlineBorder::RebuildWidget()
{
	MyBorder = SNew(SBRHairlineBorder).Params(BuildRenderParams());
	return MyBorder.ToSharedRef();
}

void UBRHairlineBorder::ApplyRenderParams()
{
	if (MyBorder.IsValid())
	{
		MyBorder->SetParams(BuildRenderParams());
	}
}

void UBRHairlineBorder::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	ApplyRenderParams();
}

void UBRHairlineBorder::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyBorder.Reset();
}

void UBRHairlineBorder::SetHairlineStyle(const FBRHairlineStyle& InStyle)
{
	HairlineStyle = InStyle;
	ApplyRenderParams();
}

void UBRHairlineBorder::SetEdgeDimmed(EBRBorderEdge Edge, bool bDimmed)
{
	const int32 EdgeBit = static_cast<int32>(Edge);
	const int32 NewMask = bDimmed ? (HairlineStyle.DimmedEdges | EdgeBit) : (HairlineStyle.DimmedEdges & ~EdgeBit);

	if (NewMask == HairlineStyle.DimmedEdges)
	{
		return;
	}

	HairlineStyle.DimmedEdges = NewMask;
	ApplyRenderParams();
}

#if WITH_EDITOR
const FText UBRHairlineBorder::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Breachpoint");
}
#endif

UBRRule::UBRRule()
{
	HairlineStyle.Edges = static_cast<int32>(EBRBorderEdge::Top);
	HairlineStyle.DimmedEdges = 0;
	HairlineStyle.SideTickLength = 0.0f;
}

void UBRRule::ApplyOrientationToEdges()
{
	// The rule's edge is DERIVED from orientation, not authored -- a rule can never accidentally
	// become a four-line border.
	HairlineStyle.Edges = (Orientation == EBRRuleOrientation::Vertical)
		? static_cast<int32>(EBRBorderEdge::Left)
		: static_cast<int32>(EBRBorderEdge::Top);
}

void UBRRule::SynchronizeProperties()
{
	ApplyOrientationToEdges();

	Super::SynchronizeProperties();
}

#undef LOCTEXT_NAMESPACE
