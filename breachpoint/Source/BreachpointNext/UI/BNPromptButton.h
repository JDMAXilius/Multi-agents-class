#pragma once

#include "CommonButtonBase.h"
#include "UI/Components/BRButton.h"
#include "BNPromptButton.generated.h"

class UCommonTextBlock;
class UImage;
class UTexture2D;

/**
 * The prompt's own style (founder, 2 Sep 2026: "make sure this is a noticeable button... if I
 * hover on that, it should highlight... make it look as an actual button"). A faint plate at
 * idle says "clickable"; hover lifts it; pressed goes to the START row's cyan. Flat, sharp
 * tints like every other fill in the front end. Padding makes the plate a pill around the
 * glyph and verb rather than a box hugging the text.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNButtonStyle_Prompt : public UBRButtonStyleBase
{
	GENERATED_BODY()

public:
	UBNButtonStyle_Prompt();
};

/**
 * `UBNPromptButton` — one entry of `Button Prompts` `21:43024` (a 20px glyph disc and a verb),
 * made clickable (founder, 2 Sep 2026: "convert those as buttons... doesn't change the visual").
 *
 * WHY NOT `UBRButtonPrompt`. That one is a LEGEND: a `CommonActionWidget` that draws the glyph
 * of an input action, and this project ships no `UCommonUIInputData`, so it would draw nothing.
 * Its glyphs here are textures drawn from the reference's own paths (`bn49_prompt_glyphs.py`).
 *
 * WHY NOT `UBRButton`. It brings a plate, four hairlines and a two-field row; the legend has
 * none of that, and the brief is "same visual". A `CommonButtonBase` with a transparent style
 * (`BRButtonStyleBase`) is a hit-box around exactly what was already drawn.
 *
 * The glyph is a per-instance SOFT reference (law 3): the same WBP is "Back" with one disc and
 * "Menu" with another, set on the instance in the screen, never in a graph.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNPromptButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** `21:43024`: glyph 20 x 20, verb at x28, 20 tall. */
	static constexpr float GlyphSize = 20.0f;
	static constexpr float VerbX = 28.0f;

	UFUNCTION(BlueprintCallable, Category = "BN|UI")
	void SetVerbText(const FText& InVerb);

protected:
	virtual void NativeOnInitialized() override;

	// BindWidget contract for `WBP_BNPromptButton`: `Glyph` (Image), `Verb` (CommonTextBlock).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UImage> Glyph;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UCommonTextBlock> Verb;

	/** Set per instance in the host screen's WBP. Applied once, at init. */
	UPROPERTY(EditAnywhere, Category = "BN|UI")
	TSoftObjectPtr<UTexture2D> GlyphTexture;
};
