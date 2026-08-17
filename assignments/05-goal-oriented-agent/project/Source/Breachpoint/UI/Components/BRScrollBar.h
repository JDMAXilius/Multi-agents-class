#pragma once

#include "CommonUserWidget.h"

#include "BRScrollBar.generated.h"

class UScrollBar;
class USizeBox;

/**
 * COMPONENT-SPECS / SCREEN-MANIFEST Sec 5 Tier 0: the reference file draws exactly two scroll
 * bars. Naming them is what stops a third appearing.
 */
UENUM(BlueprintType)
enum class EBRScrollBarWeight : uint8
{
	/** Track 8 wide, thumb 8 wide. The list-panel bar. */
	Slim,

	/** Track 13 wide, thumb 7 wide -- the thumb is INSET inside a wider rail, not flush. */
	Wide
};

/**
 * `UBRScrollBar` -- the slim/wide scroll bar (SCREEN-MANIFEST Sec 5 Tier 0: 14/31 screens).
 *
 * COMPOSITION, NOT INHERITANCE, AND WHY: the obvious move is `class UBRScrollBar : UScrollBar`.
 * It was rejected twice over. `UScrollBar` is declared `UCLASS(Experimental, MinimalAPI)` with
 * `GENERATED_UCLASS_BODY()`, so a cross-module subclass inherits an experimental contract and
 * leans on a constructor the owning module does not export -- exactly the class of failure that
 * shows up at LINK, not at compile, which is the worst possible outcome for a packet authored
 * with the editor closed. Wrapping it costs one `BindWidget` and every method used below is
 * individually `UMG_API`.
 *
 * The two measured numbers are applied from C++ (track width on the SizeBox, thumb width on the
 * bar) so a screen cannot half-adopt the design: pick a weight, get both numbers.
 *
 * THIS WIDGET DOES NOT SCROLL ANYTHING BY ITSELF. Bind it to a `UScrollBox` / `UListView` (or
 * drive it with `UScrollBar::SetState`) at the call site. A scroll bar that renders a plausible
 * thumb over a list it is not connected to is a lie about how much content there is.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRScrollBar : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** COMPONENT-SPECS Tier-0 measurement: slim is 8 x N with an 8 x 70 thumb. */
	static constexpr float SlimTrackThickness = 8.0f;
	static constexpr float SlimThumbThickness = 8.0f;

	/** COMPONENT-SPECS Tier-0 measurement: wide is 13 x N with a 7 x 70 thumb. */
	static constexpr float WideTrackThickness = 13.0f;
	static constexpr float WideThumbThickness = 7.0f;

	/** Both weights measure a 70-long thumb at rest. */
	static constexpr float ThumbLength = 70.0f;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetScrollBarWeight(EBRScrollBarWeight InWeight);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRScrollBarWeight GetScrollBarWeight() const { return Weight; }

	/** The measured track width for a weight, in design px at the 1280 authoring base. */
	static float GetTrackThickness(EBRScrollBarWeight InWeight);

	/** The measured thumb width for a weight, in design px at the 1280 authoring base. */
	static float GetThumbThickness(EBRScrollBarWeight InWeight);

	/** Expose the wrapped bar so the owning list can drive it. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	UScrollBar* GetScrollBar() const { return ScrollBar; }

protected:
	virtual void NativeOnInitialized() override;

	void ApplyWeight();

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_ScrollBar`.
	// ---------------------------------------------------------------------------------------

	/** The rail. C++ overrides its width (vertical bar) from the weight. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> Track;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UScrollBar> ScrollBar;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRScrollBarWeight Weight = EBRScrollBarWeight::Slim;

	/**
	 * Vertical is the only orientation the reference file uses (both variants are `8 x N` /
	 * `13 x N`). Kept explicit so a horizontal bar overrides the correct axis instead of
	 * silently getting a 13px-tall track.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	bool bVertical = true;
};
