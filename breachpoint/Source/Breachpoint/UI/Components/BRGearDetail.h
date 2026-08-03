#pragma once

#include "Styling/SlateBrush.h"
#include "UI/BRUITypes.h"
#include "UI/Components/BRPanel.h"

#include "BRGearDetail.generated.h"

class UCommonTextBlock;
class UImage;
class UWidget;

/**
 * SCREEN-BUILD-SPEC Sec 3.8: the detail strip "shrinks 586x161 -> 586x125 because materials carry
 * no manufacturer attribution row". That is ONE row appearing or not appearing, so it is a variant
 * on one class -- not two classes, and emphatically not a magic height typed into two WBPs.
 * The 36px delta is DERIVED from the two measured heights below, never typed.
 */
UENUM(BlueprintType)
enum class EBRGearDetailVariant : uint8
{
	/** 586 x 161 -- an equippable item, WITH the maker row. */
	Item,

	/** 586 x 125 -- a colour / finish / pattern material. No maker, so no maker row. */
	Material
};

/**
 * The focused-item detail block, pushed in one call.
 *
 * It is a struct and not three setters so a grid focus change lands ATOMICALLY: three separate
 * pushes leave frames where the name is the new item and the description is still the old one,
 * which is the class of lie `ue5-ui-architecture` Sec 7 is about. Empty text is rendered as the
 * unknown dash, never as blank.
 *
 * CONTRACT GAP -- this struct is the shape `UBRVM_Customization` must publish, and that ViewModel
 * does not exist (SCREEN-MANIFEST Sec 4, "GAP: UBRVM_Customization -- focused item detail block").
 * Until it does, nothing feeds this widget and it renders dashes, honestly.
 */
USTRUCT(BlueprintType)
struct FBRGearDetailView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText DisplayName;

	/** The manufacturer / attribution line. Empty on materials -- see `EBRGearDetailVariant`. */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText Maker;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText Description;
};

/**
 * `UBRGearDetail` -- the customization detail strip (SCREEN-MANIFEST Sec 5 Tier 3: 4/31 screens;
 * instanced by `OP_Slot`, `OP_Item`, `OP_Channel` and the `WBP_Panel_GearDetail` art-board).
 *
 * GEOMETRY, and which number is the real one: the reference component's own art-board is
 * 626 x 600 (`REFERENCE-EXTRACTION.md` Sec 5, `Gear Detail` `484:3279`), but every IN-SCREEN
 * instance is the 586-wide strip at (650, 464) (SCREEN-BUILD-SPEC Sec 2). Same discrepancy class
 * as `News Button` 330 vs feature card 349 (SCREEN-MANIFEST Sec 6.3) -- the art-board is not the
 * instance. This class is the strip; the 626 x 600 board is `UBRPanel_GearDetail`, another
 * packet's class, which composes this plus `UBRPriceTag` and `UBRTagFrame`.
 *
 * It is a `UBRPanel` because it IS one: a ground plate at a measured black alpha with a hairline
 * treatment and content. It inherits `Frame`, the ground/stroke enums and `SetPanelHeight`, which
 * is precisely what makes the 161 -> 125 variant three lines instead of a second widget.
 *
 * PUSH-ONLY AND HONESTLY UNKNOWN. It starts in `EBRUIDataState::Unknown` with every field showing
 * the em dash. It never reads `PlayerState`, an ASC, a GameState or a subsystem, and it has no
 * Tick. On a slot screen the strip is on-screen BEFORE anything is focused and again for the
 * frames after a grid rebuild -- a confident stale item name in either window tells the player
 * they are about to buy something they are not.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRGearDetail : public UBRPanel
{
	GENERATED_BODY()

public:
	/** SCREEN-BUILD-SPEC Sec 2: the in-screen instance, `Gear Detail (650,464) 586x161`. */
	static constexpr float DetailWidth = 586.0f;
	static constexpr float DetailHeight = 161.0f;

	/** SCREEN-BUILD-SPEC Sec 3.8: the terminal channel level, materials only. */
	static constexpr float MaterialDetailHeight = 125.0f;

	/** Derived, not measured and not typed: the maker row IS the difference between the two. */
	static constexpr float MakerRowHeight = DetailHeight - MaterialDetailHeight;

	/**
	 * SCREEN-BUILD-SPEC Sec 1 / SCREEN-MANIFEST Sec 6.1: x >= 650 is the 3D subject's right band,
	 * and `Gear Detail` (650->1236) is one of only TWO nodes permitted to break into it. Recorded
	 * so the exception is visible in code; the position itself is authored as a right-anchored
	 * offset in the WBP (SCREEN-MANIFEST Sec 7.3 -- column 3 anchors top-RIGHT so ultrawide is
	 * correct for free), which is why no `DetailOriginX` constant is used for layout.
	 */
	static constexpr float RightBandOriginX = 650.0f;
	static constexpr float DetailOriginY = 464.0f;

	/** `REFERENCE-EXTRACTION.md` Sec 5: the component art-board, NOT the instance. See the note above. */
	static constexpr float ComponentBoardWidth = 626.0f;
	static constexpr float ComponentBoardHeight = 600.0f;

	/** Push the focused item's detail block. Live state; empty fields still render as dashes. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetDetail(const FBRGearDetailView& InDetail);

	/** Return to the honest unknown state -- nothing focused, or focus lost during a rebuild. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearDetail();

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRUIDataState GetDataState() const { return DataState; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetVariant(EBRGearDetailVariant InVariant);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRGearDetailVariant GetVariant() const { return Variant; }

	/**
	 * The manufacturer mark that sits in the maker row (`ART-PROMPT-LIBRARY.md` Sec HJ-1).
	 * Takes an ALREADY-RESOLVED brush: law 3 keeps the art a soft ref in data, and resolving it
	 * is the owner's async load, never a widget's synchronous one. An empty brush collapses the
	 * mark and leaves the maker text alone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetMakerMark(const FSlateBrush& InBrush);

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	void ApplyVariant();

	/** Same em dash as `UBRProfileBar`, written as an escape to keep this file pure ASCII. */
	static FText GetUnknownValueText();

	/** One place empty-means-dash is decided, so three fields cannot drift apart. */
	static void ApplyFieldText(UCommonTextBlock* Field, const FText& InText);

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_GearDetail`, on top of `UBRPanel`'s `Frame` / `RootSizeBox`.
	// `RootSizeBox` is effectively required here -- without it the 161 <-> 125 variant cannot be
	// driven -- but it stays optional on the base so every other panel need not carry one.
	//
	// UNMEASURED: no document records `Gear Detail`'s internal anatomy (row origins, text
	// baselines, the mark's size). These names are chosen deliberately from the fields
	// SCREEN-MANIFEST Sec 4 lists for the detail block; read `484:3279` in
	// `Kn87U5sy2VD0lP8K7h4LcQ` before authoring the layout, and rename here if the live node
	// disagrees -- renaming later costs one WBP rebind, inventing a number costs a redesign.
	// ---------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> ItemName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Description;

	/** The whole attribution row. Collapsed on `Material` -- that IS the 36px. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> MakerRow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> MakerName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> MakerMark;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRGearDetailVariant Variant = EBRGearDetailVariant::Item;

private:
	/** Starts Unknown and stays Unknown until something pushes. Nothing here polls to find out. */
	EBRUIDataState DataState = EBRUIDataState::Unknown;
};
