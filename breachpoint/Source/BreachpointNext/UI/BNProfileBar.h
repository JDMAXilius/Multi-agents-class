#pragma once

#include "CommonUserWidget.h"
#include "BNProfileBar.generated.h"

class UCommonTextBlock;
class UImage;
class UPanelWidget;
class USizeBox;
class UTexture2D;

/**
 * `UBNProfileBar` — the footer identity strip, as ONE component.
 *
 * WHY A COMPONENT AND NOT SIX CANVAS WIDGETS. BN42 originally hand-placed a Border, an avatar,
 * a name and three glyphs onto `WBP_BNScreen_FrontEnd`'s canvas. That is six widgets and six
 * absolute positions re-derived on every screen that wants a footer, and it is exactly the
 * mistake the roster and the nav bar were already caught making. This is the same shape as the
 * shipped `BR` components: a measured box, a BindWidget contract, and data pushed in through
 * one function.
 *
 * WHY IT IS NOT `UBRProfileBar`. That class was CUT (`MCP-BUILD-PLANS.md` §B5) — "zero
 * referencing classes, `SetIdentity` has zero callers, and `WBP_RootLayout` has no chrome slot
 * for the bar to live in yet". Two of those three are still true, so reviving it in
 * `Source/Breachpoint/` would be reviving a cut class outside this packet's owner path. This is
 * BN's own, in BN's module, and it can be deleted or folded into the `BR` one the day
 * `WBP_RootLayout` grows the chrome slot the doctrine wants
 * (`SCREEN-MANIFEST.md:612-615` — authored once, not per screen).
 *
 * THE PREMISE CORRECTION THAT MATTERS. `Profile Bar` `119:1525` is 1280 x 50 containing exactly
 * ONE child: `Player Card` at **x862, 349 x 50** — column 3's origin and column 3's width, so it
 * lines up under the roster panel. It is NOT a full-width row of content. A 1280-wide HBox
 * stretches a 349-wide design, which is why the numbers below place a card rather than fill a bar.
 */
// Config = Game is LOAD-BEARING, not decoration: UHT refuses to compile a class that declares
// UPROPERTY(Config) without it ("Classes with config member variables need to specify config
// file"). That is the compiler catching, for free, the exact bug filed against four BR classes
// in this ticket - their ini lines are dead because they used EditAnywhere/EditDefaultsOnly
// instead of Config, so UHT had nothing to object to and the failure moved to runtime.
UCLASS(Abstract, Config = Game, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNProfileBar : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Measured: `Profile Bar` `21:32862` on FE_Play — 0,670 1280 x 50, fill `#000000@0.5`. */
	static constexpr float BarWidth = 1280.0f;
	static constexpr float BarHeight = 50.0f;

	/** Measured: the bar's only child, `Player Card`, at x862 — column 3's origin. */
	static constexpr float CardOriginX = 862.0f;
	static constexpr float CardWidth = 349.0f;

	/** Card-local, BP73. Avatar is a 40 x 40 SQUARE at a 5px inset — the round look is art. */
	static constexpr float AvatarSize = 40.0f;
	static constexpr float AvatarInset = 5.0f;

	/** Card-local (55,17) 107 x 17. The 55 is 5 + 40 + a 10 gap, not a free number. */
	static constexpr float GamertagX = AvatarInset + AvatarSize + 10.0f;
	static constexpr float GamertagY = 17.0f;
	static constexpr float GamertagWidth = 107.0f;
	static constexpr float GamertagHeight = 17.0f;

	/** Card-local (211,0) 122 x 50; 211 + 122 = 333, i.e. the card's 16px right inset. */
	static constexpr float ButtonsX = 211.0f;
	static constexpr float ButtonsWidth = 122.0f;

	/** Three cells across the 122. UNMEASURED — the per-glyph boxes are NOT RECORDED anywhere. */
	static constexpr int32 ButtonCount = 3;

	/** Push identity in. Soft avatar ref (law 3); a null ref collapses the image, never a box. */
	UFUNCTION(BlueprintCallable, Category = "BN|UI")
	void SetIdentity(const FText& InGamertag, const TSoftObjectPtr<UTexture2D>& InAvatar);

protected:
	virtual void NativeOnInitialized() override;

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_BNProfileBar`. Figma layer -> UMG name:
	//   `Profile Bar` -> RootSizeBox   (the 1280 x 50 shell)
	//   fill          -> Ground        (#000000@0.5; the reference also blurs, see the note)
	//   `Superintendent` -> Avatar     `Gamer and Service Tag` -> Gamertag
	//   `Buttons`     -> ButtonsBox
	// -------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UImage> Ground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UImage> Avatar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UCommonTextBlock> Gamertag;

	/** The three glyph cells. C++ only sizes it; the cells themselves are the WBP's. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UPanelWidget> ButtonsBox;

	/** Config, so the footer's identity is data and not a string typed into a .uasset. */
	UPROPERTY(Config)
	FString DefaultGamertag;

	UPROPERTY(Config)
	TSoftObjectPtr<UTexture2D> DefaultAvatar;

private:
	/** Held so a late `SetIdentity` cannot be clobbered by a stale async avatar load. */
	UPROPERTY(Transient)
	TSoftObjectPtr<UTexture2D> PendingAvatar;
};
