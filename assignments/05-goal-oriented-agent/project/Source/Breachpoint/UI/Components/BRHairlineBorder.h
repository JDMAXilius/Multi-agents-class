#pragma once

#include "Components/Widget.h"
#include "UI/Components/BRComponentTokens.h"
#include "Widgets/SLeafWidget.h"

#include "BRHairlineBorder.generated.h"

/**
 * COMPONENT-SPECS Sec 2: the border is FOUR separate vector lines with independent opacities --
 * a full top line, a dimmed bottom line, and two short side ticks. It is never a closed box.
 */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EBRBorderEdge : uint8
{
	None = 0 UMETA(Hidden),
	Top = 1,
	Bottom = 2,
	Left = 4,
	Right = 8
};

/**
 * The measured description of one hairline plate. Everything here is APPEARANCE sourced from
 * COMPONENT-SPECS; there is no gameplay number and no hex literal in this struct.
 */
USTRUCT(BlueprintType)
struct FBRHairlineStyle
{
	GENERATED_BODY()

	/**
	 * Which of the four edges draw at all. COMPONENT-SPECS Sec 2 draws all four on a menu row.
	 * Default 15 = Top|Bottom|Left|Right (1|2|4|8). Literal because UHT parses struct
	 * initialisers verbatim; the bit values are on EBRBorderEdge above.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI",
		meta = (Bitmask, BitmaskEnum = "/Script/Breachpoint.EBRBorderEdge"))
	int32 Edges = 15;

	/**
	 * Which of the drawn edges use DimStrokeToken instead of StrokeToken.
	 * COMPONENT-SPECS Sec 2 idle state: top at 1.0, bottom + both ticks at 0.3.
	 * Hover clears Bottom from this mask -- that is the whole "0.3 -> 1.0" transition.
	 * Default 14 = Bottom|Left|Right (2|4|8).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI",
		meta = (Bitmask, BitmaskEnum = "/Script/Breachpoint.EBRBorderEdge"))
	int32 DimmedEdges = 14;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	EBRUIColorToken StrokeToken = EBRUIColorToken::ChromeStroke;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	EBRUIColorToken DimStrokeToken = EBRUIColorToken::ChromeStrokeDim;

	/** Optional solid plate behind the strokes. `None` = no fill (the normal border case). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	EBRUIColorToken FillToken = EBRUIColorToken::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	EBRStrokeWeight Weight = EBRStrokeWeight::Chrome;

	/**
	 * COMPONENT-SPECS Sec 2: the side lines are `0 x 20` on a 28-tall row -- short vertically
	 * centred TICKS, not full-height edges. 0 means "run the full height".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI", meta = (ClampMin = "0.0"))
	float SideTickLength = 20.0f;
};

/** Token-resolved, Slate-facing form of FBRHairlineStyle. An edge draws iff its alpha > 0. */
struct FBRHairlineRenderParams
{
	FLinearColor FillColor = FLinearColor::Transparent;
	FLinearColor TopColor = FLinearColor::Transparent;
	FLinearColor BottomColor = FLinearColor::Transparent;
	FLinearColor LeftColor = FLinearColor::Transparent;
	FLinearColor RightColor = FLinearColor::Transparent;
	float Thickness = BRUI::ChromeWeightPx;
	float SideTickLength = 0.0f;
};

/**
 * The leaf that draws every hairline in the game. Four FSlateDrawElement::MakeLines calls plus
 * an optional fill box; no brush asset, no texture, no child widgets.
 */
class BREACHPOINT_API SBRHairlineBorder : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBRHairlineBorder) {}
		SLATE_ARGUMENT(FBRHairlineRenderParams, Params)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetParams(const FBRHairlineRenderParams& InParams);

protected:
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	FBRHairlineRenderParams Params;
};

/**
 * `UBRHairlineBorder` -- the atom under the atoms (SCREEN-MANIFEST Sec 5 Tier 0, listed there as
 * `UBRButtonBorder`; same component, this is the class name the packet fixed).
 *
 * WHY A `UWidget` OVER A SLATE LEAF, NOT A `UCommonUserWidget` COMPOSITE
 * ---------------------------------------------------------------------
 * COMPONENT-SPECS Sec 0 counts 404 x 1px, 323 x 0.5px and 177 x 2px strokes in the library. The
 * composite alternative -- a UserWidget holding four `UImage` children -- costs one WBP asset,
 * five widgets and five Slate draw elements PER BORDER, i.e. ~4500 widgets across the front
 * end, and it needs a Blueprint asset before a single line can be drawn (R18/R26 pressure for
 * zero benefit). The leaf draws the same four lines in four draw elements with zero widgets,
 * zero assets and zero texture memory. This is the single biggest asset-count saving in the
 * front-end plan, which is exactly why it is built once here.
 *
 * The cost of the choice, stated honestly: a leaf cannot hold a WidgetAnimation. Anything that
 * must ANIMATE a border animates the owning WBP's render opacity / the owner's C++-driven
 * token swap (see `UBRButton::ApplyInvertedState`), not the border's internals.
 *
 * This is a decoration primitive, so it is `HitTestInvisible` by default -- it must never eat a
 * click meant for the button it decorates. `UBRScrim` deliberately overrides that.
 */
UCLASS(meta = (DisableNativeTick))
class BREACHPOINT_API UBRHairlineBorder : public UWidget
{
	GENERATED_BODY()

public:
	UBRHairlineBorder();

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetHairlineStyle(const FBRHairlineStyle& InStyle);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	FBRHairlineStyle GetHairlineStyle() const { return HairlineStyle; }

	/**
	 * COMPONENT-SPECS Sec 2 hover: `SetEdgeDimmed(Bottom, false)` IS the 0.3 -> 1.0 inversion.
	 * Not a UFUNCTION on purpose: EBRBorderEdge is a bitflags enum and deliberately not
	 * BlueprintType, and nothing in a graph-free WBP calls this -- C++ owners do.
	 */
	void SetEdgeDimmed(EBRBorderEdge Edge, bool bDimmed);


	//~ Begin UWidget interface
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif
	//~ End UWidget interface

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	FBRHairlineRenderParams BuildRenderParams() const;

	/** Resolve tokens and push to Slate. Cheap; safe before the Slate widget exists. */
	void ApplyRenderParams();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI", meta = (ShowOnlyInnerProperties))
	FBRHairlineStyle HairlineStyle;

	TSharedPtr<SBRHairlineBorder> MyBorder;
};

UENUM(BlueprintType)
enum class EBRRuleOrientation : uint8
{
	Horizontal,

	Vertical
};

/**
 * `UBRRule` -- the decorative line (SCREEN-MANIFEST Sec 5 Tier 0, 12/31 screens).
 *
 * It is `UBRHairlineBorder` with exactly one edge enabled, so it reuses the one paint path
 * rather than adding a second. Its orientation drives which edge that is, and its desired size
 * is the stroke weight on the minor axis -- so a rule in a Fill-aligned box slot stretches along
 * its length and stays 0.5 / 1 / 2 px thick across it, with no hand-typed size.
 */
UCLASS(meta = (DisableNativeTick))
class BREACHPOINT_API UBRRule : public UBRHairlineBorder
{
	GENERATED_BODY()

public:
	UBRRule();


	virtual void SynchronizeProperties() override;

protected:
	void ApplyOrientationToEdges();

	/** Derived: Horizontal draws the Top edge, Vertical draws the Left edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRRuleOrientation Orientation = EBRRuleOrientation::Horizontal;
};
