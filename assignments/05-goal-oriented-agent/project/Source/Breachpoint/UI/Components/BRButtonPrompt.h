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
	/**
	 * COMPONENT-SPECS Sec 6: prompts are h20 and their WIDTH HUGS — the measured 62/146/253 at
	 * 1/2/3 prompts is the expected *result* of hugging, recorded in `MCP-BUILD-PLANS.md` with
	 * the rest of the WBP measurements (CPP-AUDIT cut the six constants that mirrored them here
	 * unread — a constexpr no code reads and no test asserts is a comment with a type).
	 */
	static constexpr float PromptHeight = 20.0f;

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
