#pragma once

#include "CommonUserWidget.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRProfileBar.generated.h"

class UBackgroundBlur;
class UCommonTextBlock;
class UImage;
class UPanelWidget;
class USizeBox;

/**
 * `UBRProfileBar` -- the always-present bottom band (SCREEN-MANIFEST Sec 5 Tier 0: 31/31 screens).
 *
 * COMPONENT-SPECS Sec 6: 1280 x 50 at (0, 670), `#000000@0.5` + BACKGROUND_BLUR.
 * ui-presentation Sec 5: "Safe bottom 720 - 670 = 50 reserved for the profile bar, ALWAYS."
 * SCREEN-MANIFEST Sec 7.1: it lives in `WBP_RootLayout`'s chrome slot, not in each screen --
 * re-authoring it per screen is 31 places one blur setting drifts.
 *
 * REPLICATION TIMING (`ue5-ui-architecture` Sec 7): this bar is on screen during travel, during
 * the frames where `PlayerState` is null, and before the ASC has run `InitAbilityActorInfo`.
 * It therefore NEVER reads a player, a controller or a ViewModel -- it renders whatever it was
 * last pushed, and it starts life showing the unknown dash rather than a confident empty name.
 * Every field is push-only via `SetIdentity`; there is no getter into game state and no Tick.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRProfileBar : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** COMPONENT-SPECS Sec 6 / ui-presentation Sec 5: 1280 x 50 at y = 670. */
	static constexpr float BarHeight = 50.0f;
	static constexpr float BarOriginY = 670.0f;
	static constexpr float DesignCanvasHeight = 720.0f;

	/**
	 * Push the local player's identity in. Empty text is rendered as the unknown dash, not as
	 * blank -- a blank gamertag reads as "no one is signed in", which is a different, false
	 * statement.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetIdentity(const FText& InGamertag, const FText& InStatus);

	/** Return to the honest unknown state. Called on construct and on travel. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearIdentity();

protected:
	virtual void NativeOnInitialized() override;

	static FText GetUnknownValueText();

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_ProfileBar`.
	// ---------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/**
	 * COMPONENT-SPECS Sec 7 "Panel blur": BACKGROUND_BLUR on the profile bar.
	 * UNMEASURED: the reference file records the effect but not its radius, so the radius is
	 * left entirely to this widget's WBP / the shared style asset. C++ deliberately does not
	 * invent a number for it.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBackgroundBlur> Blur;

	/** The `#000000@0.5` plate. C++ tints it from a token so no hex reaches the WBP. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Ground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Gamertag;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Status;

	/**
	 * COMPONENT-SPECS Sec 6 places the button prompts at (60, 685) -- inside this band. The bar
	 * owns the hugging container; the screen stack fills it with `UBRButtonPrompt` instances.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> PromptContainer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken GroundToken = EBRUIColorToken::PanelGround50;
};
