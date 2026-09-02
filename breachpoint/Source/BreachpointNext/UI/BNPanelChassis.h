#pragma once

#include "CommonUserWidget.h"
#include "BNPanelChassis.generated.h"

class UImage;
class UNamedSlot;
class USizeBox;

/**
 * `UBNPanelChassis` — the bordered-panel look every 349-wide column shares, as ONE component.
 *
 * MEASURED off `Menu in Border` `I21:43047;7:7383` on CG_Lobby, then sampled from Figma's own
 * render of that node (numbers are the render's greys on its light board; the tokens are the
 * in-game values):
 *
 *     plate      the panel fill, translucent dark               -> PanelGround (#000000 @ 0.5)
 *     frame      a 3px band all the way round (142) with a
 *                1px brighter core at x1 (165)                  -> ChromeStroke, dim + core
 *     notches    `Rectangle 258` 88 x 4.727 at list-local (127, 0)
 *                `Rectangle 259` 88 x 4.727 at list-local (215, 180)
 *                — they are FRAME-COLOURED TABS reaching into the plate, not bright bars.
 *                The render is unambiguous: the notch reads 142 straight down through y1..y6,
 *                the same value as the frame, where BN had been drawing a 90%-white bar.
 *     caret      `Rectangle 278` 3 x 65 at list-local (-4, 58) — 165/183/165, BRIGHTER than
 *                the frame, standing one pixel proud of its left edge.
 *     contents   inset 3 (the frame) + 16 = 19 from the panel edge, 311 wide.
 *
 * WHY A COMPONENT. This exact chassis is drawn by the front end's menu, the lobby's menu, the
 * lobby's roster column and the front end's party list — four places, and BN had it as loose
 * canvas Images in two of them and nothing in the other two. One WBP, one NamedSlot for whatever
 * goes inside, and the chassis is authored once.
 *
 * WHY NOT `UBRPanel`. Its frame is a `UBRHairlineBorder` — four 1px lines. The reference frame
 * is a 3px BAND, and the notches and caret are not lines at all. Different shape, so a different
 * component rather than a hairline pushed past what it draws.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNPanelChassis : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Every instance of this chassis in the reference is one column wide. */
	static constexpr float PanelWidth = 349.0f;

	/** The frame band; the list sits inside it, so the list is 343 wide. */
	static constexpr float FrameWidth = 3.0f;

	/** Contents sit a further 16 inside the list: 19 from the panel edge, 311 wide. */
	static constexpr float ContentInset = 16.0f;

	/** `Rectangle 258/259`. List-local x; add FrameWidth for panel-local. */
	static constexpr float NotchWidth = 88.0f;
	static constexpr float NotchHeight = 4.727f;
	static constexpr float NotchTopX = 127.0f;
	static constexpr float NotchBottomX = 215.0f;

	/** `Rectangle 278`: 3 x 65 at list-local (-4, 58). The -4 puts it 1px proud of the frame. */
	static constexpr float CaretWidth = 3.0f;
	static constexpr float CaretHeight = 65.0f;
	static constexpr float CaretX = -4.0f;
	static constexpr float CaretY = 58.0f;

	/** Panels are fixed-height by design; the roster's is 599, the menu's 186. */
	UFUNCTION(BlueprintCallable, Category = "BN|UI")
	void SetPanelHeight(float InHeight);

	/** The caret marks the focused row on the reference; hide it on panels that have none. */
	UFUNCTION(BlueprintCallable, Category = "BN|UI")
	void SetCaretVisible(bool bVisible);

protected:
	virtual void NativeOnInitialized() override;

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_BNPanelChassis`. Figma layer -> UMG name:
	//   `Menu in Border` -> RootSizeBox     `Rectangle 257` -> Ground
	//   `Rectangle 258`  -> NotchTop        `Rectangle 259` -> NotchBottom
	//   `Rectangle 278`  -> Caret           `Contents`      -> Content (NamedSlot)
	// -------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UImage> Caret;

	/** Whatever the panel holds. The chassis never looks inside it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UNamedSlot> Content;
};
