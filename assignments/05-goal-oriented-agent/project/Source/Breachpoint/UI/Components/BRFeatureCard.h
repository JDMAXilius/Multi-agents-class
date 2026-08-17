#pragma once

#include "CommonButtonBase.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRFeatureCard.generated.h"

class UBRHairlineBorder;
class UCommonTextBlock;
class UImage;
class UPanelWidget;
class USizeBox;
class UTexture2D;
class UWidgetAnimation;
struct FStreamableHandle;

/**
 * `UBRFeatureCard` -- the `News Button` (SCREEN-MANIFEST Sec 5 Tier 2 / Sec 6.2: 4 of 31 screens).
 *
 * COMPONENT-SPECS Sec 6: 349 x 222, fill `#000000@0.5`.
 * ui-presentation Sec 5: "Feature card 349 x 222 (image 349 x 196.7 + caption)".
 * It is the top block of `UBRLeftRail` and the entry point to the `NW_*` article screens.
 *
 * WHY `UCommonButtonBase`: the card is pressable and it is the FIRST focus target in column 1,
 * i.e. the gamepad's entry into the rail. CommonUI already owns focus, hover, press and the
 * input-action glyph; re-deriving any of it here would be a gamepad-routing finding
 * (`ue5-ui-architecture` Sec 6). `UCommonButtonBase` IS a `UCommonUserWidget`, so there is still
 * exactly one widget base.
 *
 * RECORDED DISCREPANCY (SCREEN-MANIFEST Sec 9 q13, open): the component art-board measures
 * **330** wide while the in-screen instance measures **349** -- the same discrepancy class as
 * `Player Buttons` 390/349. This class builds to **349** and HUGS: only the HEIGHT is driven
 * from C++, the width comes from the rail's fixed 349 column. Pinning a width here would make
 * the unresolved 330 permanent in every screen that instances the card. `ComponentBoardWidth`
 * below records the losing number so a reviewer can see the call was made, not missed.
 *
 * THE IMAGE IS A SOFT REFERENCE, ALWAYS (law 3). The carousel swaps art at runtime, so the
 * texture arrives as `TSoftObjectPtr` and is streamed in asynchronously. There is no
 * `ConstructorHelpers`, no hard `UTexture2D*` UPROPERTY and no synchronous load on the swap --
 * a sync load on the front end's carousel is a visible hitch on the least excusable screen.
 *
 * HONEST EMPTY STATE (`ue5-ui-architecture` Sec 7): before the texture resolves, and whenever
 * the card has no entry, the image is `Hidden` (NOT `Collapsed` -- the 196.7 band keeps its
 * space so the caption and the rail below it never jump) and the caption renders the unknown
 * dash rather than an empty line that reads as "there is no news".
 *
 * NO GRAPH NODES (`ui-presentation` Sec 11): state comes from C++; the WBP contributes layout
 * plus an OPTIONAL `BindWidgetAnim` this class plays.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRFeatureCard : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** COMPONENT-SPECS Sec 6 / figma_geometry `UBRLeftRail.children.feature_card` (node 0:933).
	 *  Width is the rail's business (the 330-vs-349 story is in the class comment); the two
	 *  heights below are what C++ actually drives. */
	static constexpr float CardHeight = 222.0f;

	/** ui-presentation Sec 5: "image 349 x 196.7 + caption". */
	static constexpr float ImageHeight = 196.7f;

	/**
	 * Push one carousel entry's art in. The reference is resolved asynchronously; a second call
	 * before the first resolves wins, and the stale load is dropped on arrival rather than
	 * flashing the previous article's image.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetFeatureImage(TSoftObjectPtr<UTexture2D> InImage);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetCaptionText(const FText& InCaption);

	/** Return to the honest unknown state: no art, dashed caption. Called on construct. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearFeature();

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	//~ Begin UCommonButtonBase interface
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	//~ End UCommonButtonBase interface

	/** COMPONENT-SPECS Sec 1: hover is an inversion cue, expressed once for both callers. */
	void ApplyHoverState(bool bHovered);

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_FeatureCard`. Parsed out of this header by
	// `Tools/gen_ui/wbp_plan.py`; these names must match the WBP's widget names exactly.
	// Figma layer -> UMG name mapping (node 0:933 `News Button`):
	//   the `#000000@0.5` plate -> Ground        the 349x196.7 art band -> ImageBox / FeatureImage
	//   the caption line        -> Caption       the `Switcher` dots slot -> DotsContainer
	// ---------------------------------------------------------------------------------------

	/** Height only. Width hugs the rail's 349 column -- see the q13 note in the class comment. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/** COMPONENT-SPECS Sec 6: fill `#000000@0.5`. C++ tints it so no hex reaches the WBP. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Ground;

	/** The 196.7-tall art band. Reserves its height even with no art, so nothing reflows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> ImageBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UImage> FeatureImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Caption;

	/** Optional: the card's hairline. Its bottom line un-dims on hover, as on a menu row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> Border;

	/** SCREEN-MANIFEST Sec 6.4: `Switcher` 72 x 10 -- an instance slot, filled by the screen. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> DotsContainer;

	/** Played forward on hover, reverse on unhover. Appearance only. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> HoverAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken GroundToken = EBRUIColorToken::PanelGround50;

private:
	void HandleImageLoaded(FSoftObjectPath LoadedPath);

	/** The entry currently requested. The async callback compares against this to drop stragglers. */
	UPROPERTY(Transient)
	TSoftObjectPtr<UTexture2D> RequestedImage;

	/** Kept alive for the duration of the request: dropping it lets the streamer cancel the load. */
	TSharedPtr<FStreamableHandle> ImageHandle;
};
