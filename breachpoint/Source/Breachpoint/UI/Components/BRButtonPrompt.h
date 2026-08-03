#pragma once

#include "CommonUserWidget.h"

#include "BRButtonPrompt.generated.h"

class UCommonActionWidget;
class UCommonTextBlock;
class UInputAction;
class USizeBox;

/**
 * `UBRButtonPrompt` -- glyph + verb (SCREEN-MANIFEST Sec 5 Tier 0: on all 31 screens).
 *
 * WIDTH IS NEVER FIXED. COMPONENT-SPECS Sec 6 measures the prompt at 62 x 20 at (60, 685), but
 * the bar around it is 58/62 wide for one prompt, 133/146 for two and 253 for three -- those
 * numbers are an OUTPUT of hugging content, not an input. Only the HEIGHT is driven from C++;
 * the width comes from the glyph plus the verb, and the container is a hugging
 * `UHorizontalBox` in `WBP_RootLayout`'s chrome slot (SCREEN-MANIFEST Sec 7.1/Sec 7.3: bottom-left
 * anchor, "size to content"). Anything that types 62 into a width is a defect.
 *
 * The glyph is `UCommonActionWidget`, so the platform icon swaps itself when the input device
 * changes -- the prompt never hard-codes a key or a controller face button
 * (`ue5-ui-architecture` Sec 6: the UI never binds a raw key).
 *
 * Not activatable: a prompt affects nothing about input routing, and Epic's guidance is to be
 * activatable only when you need to. It is a `UCommonUserWidget`.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRButtonPrompt : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** COMPONENT-SPECS Sec 6: Button Prompts 62 x 20 at (60, 685). */
	static constexpr float PromptHeight = 20.0f;

	/** Nominal single-prompt width. Recorded for review, NOT applied -- see the class comment. */
	static constexpr float NominalPromptWidth = 62.0f;

	/** SCREEN-MANIFEST Sec 7.3: the prompt row anchors bottom-left at x=74, y=-35. */
	static constexpr float PromptRowOriginX = 74.0f;
	static constexpr float PromptRowOffsetY = -35.0f;

	/**
	 * Measured bar widths for 1 / 2 / 3 prompts (58-62, 133-146, 253). These are the EXPECTED
	 * RESULT of hugging, kept here so a reviewer can check the hug rather than so anyone can
	 * set them.
	 */
	static constexpr float ExpectedBarWidthOnePrompt = 62.0f;
	static constexpr float ExpectedBarWidthTwoPrompts = 146.0f;
	static constexpr float ExpectedBarWidthThreePrompts = 253.0f;

	/**
	 * Show this prompt for an input action. A null action collapses the widget rather than
	 * rendering a blank glyph -- an empty prompt slot tells the player something false about
	 * what the screen accepts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetPrompt(UInputAction* InAction, const FText& InVerb);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearPrompt();

protected:
	virtual void NativeOnInitialized() override;

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_ButtonPrompt`.
	// ---------------------------------------------------------------------------------------

	/** Height only. Width is left to hug. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonActionWidget> ActionGlyph;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Verb;
};
