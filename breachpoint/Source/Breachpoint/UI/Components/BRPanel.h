#pragma once

#include "CommonUserWidget.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRPanel.generated.h"

class UBRHairlineBorder;
class UNamedSlot;
class USizeBox;

/**
 * `UBRPanel` -- the bordered container the whole design language is built from.
 *
 * WHAT IT IS, AND WHAT IT IS DELIBERATELY NOT
 * -------------------------------------------
 * The Figma page `Panels & Cards` (`6:27`) holds 10 component sets + 10 singles = 45 variants.
 * They are not 45 classes and they are not even 20. Every one of them is the same three things:
 *   1. a ground plate at one of FIVE black alphas (COMPONENT-SPECS Sec 8 -- 0.2/0.4/0.5/0.6/0.8,
 *      "five discrete alphas, not arbitrary"),
 *   2. a hairline treatment at one of THREE weights (COMPONENT-SPECS Sec 0 -- 404 x 1px,
 *      323 x 0.5px, 177 x 2px, and no fourth weight exists anywhere in the file),
 *   3. content.
 * Both axes are enums here for exactly that reason: a free float would let a 46th ground alpha
 * and a fourth stroke weight be typed into a details panel, and the count above is the whole
 * argument for one class instead of forty-five.
 *
 * WHY THERE IS NO `Ground` IMAGE
 * ------------------------------
 * `UBRHairlineBorder` already draws an optional token-coloured fill behind its strokes
 * (`FBRHairlineStyle::FillToken`). A panel's ground and its border are therefore ONE widget and
 * one Slate draw element, not a `UImage` plus four lines. Redrawing either here would fork the
 * one paint path that Sec 0's ~900 strokes are budgeted against.
 *
 * CORNER RADIUS IS ZERO. COMPONENT-SPECS Sec 0 / SCREEN-MANIFEST Sec 7.4: "Sharp corners are the
 * language; a rounded panel is a defect." The only radii in the reference file are on 17 badges.
 * `UBRHairlineBorder` has no radius to set, which is the enforcement -- `PanelCornerRadius` below
 * exists so the number is stated once, with its source, rather than assumed.
 *
 * PUSH-ONLY, LIKE EVERY COMPONENT IN THIS FOLDER. A panel reads nothing. It has no ViewModel, no
 * `PlayerState`, no ASC, no GameState, no Tick (`ue5-ui-architecture` Sec 7/8) -- its owner pushes
 * appearance in. It is also decoration, so it takes no focus: the interactive children it wraps
 * (`UBRMenuRow` and friends) own gamepad navigation, and a panel that grabbed focus would put a
 * dead stop in a controller path.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRPanel : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * COMPONENT-SPECS Sec 0 + SCREEN-MANIFEST Sec 7.4. Stated, not used: nothing in the hairline
	 * paint path can round a corner. If a panel ever renders rounded, the defect is in the WBP's
	 * brush, not here.
	 */
	static constexpr float PanelCornerRadius = 0.0f;

	/** COMPONENT-SPECS Sec 6: the design canvas every measured panel origin is quoted against. */
	static constexpr float DesignCanvasWidth = 1280.0f;
	static constexpr float DesignCanvasHeight = 720.0f;

	/** COMPONENT-SPECS Sec 8: one of the five measured black alphas. Never a free float. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetGroundToken(EBRUIColorToken InGroundToken);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRUIColorToken GetGroundToken() const { return GroundToken; }

	/** COMPONENT-SPECS Sec 0: 0.5 / 1 / 2 px. There is no fourth weight in the reference file. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetStrokeWeight(EBRStrokeWeight InStrokeWeight);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRStrokeWeight GetStrokeWeight() const { return StrokeWeight; }

	/**
	 * Height-only, because that is the only dimension any measured panel variant changes
	 * (`UBRGearDetail` 161 -> 125, SCREEN-BUILD-SPEC Sec 3.8). Width is layout: SCREEN-MANIFEST
	 * Sec 8.2 -- "the columns never stretch", 349 / 504 / 536 / 586 are instrument widths authored
	 * in the WBP. `InHeight <= 0` clears the override and returns the panel to its WBP height.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetPanelHeight(float InHeight);

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	/**
	 * Pushes GroundToken + StrokeWeight into `Frame`. It deliberately does NOT touch `Edges`,
	 * `DimmedEdges` or `SideTickLength`: which edges a given card draws is measured per component
	 * (COMPONENT-SPECS Sec 2 draws four, Sec 5 draws four at 2px, the Party List of
	 * SCREEN-MANIFEST Sec 6.3 draws a closed 1px INSIDE stroke) and is authored on the WBP's
	 * `Frame`. Only the two axes that are enumerable across all 45 variants are driven from C++.
	 */
	void ApplyPanelStyle();

	// ---------------------------------------------------------------------------------------
	// BindWidget contract. A generator parses these names out of this header, so they are a
	// contract, not a suggestion. Every `WBP_*` parented to `UBRPanel` (directly or through a
	// subclass) must carry `Frame`; `RootSizeBox` and `Content` are optional by design.
	// ---------------------------------------------------------------------------------------

	/**
	 * The ground AND the border, in one widget. Required: a panel without a `Frame` has no way to
	 * express its ground alpha, and a WBP that draws its own plate has forked the palette.
	 * A frame with `Edges = 0` is the legal "ground only" card.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> Frame;

	/** Present only on panels whose height is driven (see `SetPanelHeight`). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/**
	 * Where an INSTANCED panel's caller drops its content. Panels that are subclassed rather than
	 * instanced (`UBRGearDetail`) author their content directly and leave this unbound.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UNamedSlot> Content;

	/** COMPONENT-SPECS Sec 8: panel grounds are five discrete black alphas. 0.5 is the commonest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken GroundToken = EBRUIColorToken::PanelGround50;

	/** COMPONENT-SPECS Sec 0: 1px chrome is the default; 404 of the file's strokes use it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRStrokeWeight StrokeWeight = EBRStrokeWeight::Chrome;
};
