#pragma once

#include "UI/BRActivatableWidget.h"
#include "UI/Components/BRMenuRow.h"

#include "BRModal_Options.generated.h"

class UBRScrim;
class UCommonTextBlock;
class UPanelWidget;
class USizeBox;

/** Fires when the player commits to a row. Single-cast: one caller raised the modal, one is told. */
DECLARE_DELEGATE_OneParam(FBROptionsRowChosen, FName /* RowId */);

/**
 * SCREEN-BUILD-SPEC Sec 3.9 / SCREEN-MANIFEST Sec 4.8: `OV_PopupOptions` (`2387:32475`) and
 * `OV_Filters` / `Gear Filters` (`891:8026`) are the SAME 451 x 682 footprint at x=48 over a full
 * scrim. They are one class with two variants, not two screens. The only thing that actually
 * differs is what a choice means.
 */
UENUM(BlueprintType)
enum class EBRModalOptionsVariant : uint8
{
	/** `OV_PopupOptions`: a row is an action. Choosing one answers the caller and pops. */
	Actions,

	/** `OV_Filters`: a row is a toggle. Choosing one reports it and the modal STAYS OPEN. */
	Filters
};

/** One row of the payload. Maps 1:1 onto `UBRMenuRow`'s Type axis -- the modal invents no widget of its own. */
USTRUCT(BlueprintType)
struct FBROptionsRow
{
	GENERATED_BODY()

	/** What comes back to the caller. The modal never interprets it. */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FName RowId;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText Label;

	/** COMPONENT-SPECS Sec 2: the right-aligned `Selection` text -- the current value on a row. */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText Value;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	EBRMenuRowType RowType = EBRMenuRowType::Default;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	bool bEnabled = true;
};

/**
 * The payload `UBRModal_Options` takes from the caller, in place of a ViewModel (SCREEN-MANIFEST
 * Sec 4.8: "takes a row payload from the caller"). The caller owns the meaning of every row; the
 * modal owns only how they look and which one was picked.
 */
USTRUCT(BlueprintType)
struct FBROptionsRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText Title;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	EBRModalOptionsVariant Variant = EBRModalOptionsVariant::Actions;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	TArray<FBROptionsRow> Rows;

	/**
	 * Deliberately NOT a `UPROPERTY` -- a reflected binding here would be gameplay logic reachable
	 * from a widget graph. Bind with `CreateUObject` so it dies with the caller.
	 */
	FBROptionsRowChosen OnRowChosen;
};

/**
 * `UBRModal_Options` -- `OV_PopupOptions` `2387:32475` (451 x 682) and `Gear Filters` `891:8026`,
 * reference file `Kn87U5sy2VD0lP8K7h4LcQ` (SCREEN-MANIFEST Sec 4.8). WBP: `WBP_Modal_Options`.
 * Pushes to `Layer.Modal`.
 *
 * IT IS NOT A NAVIGATION LEVEL. It layers over whatever grid is beneath it and is deliberately
 * off-grid (Sec 1: "451 -- modal, deliberately off-grid"). `FB_CardsFilters` and
 * `FB_TableSelectOptions` are this modal over `WBP_Screen_Browser`, not screens of their own --
 * which is why 8 browser frames collapse to 1 screen + this.
 *
 * Ultrawide: it stays 451 wide at x=48 (Sec 8.2 -- "a modal that stretches to 21:9 is
 * unreadable"). The width is driven from C++ onto `RootSizeBox` so it cannot be stretched from a
 * details panel; the x=48 / vertical-centre anchor is the WBP's job (Sec 7.3).
 *
 * Rows are `UBRMenuRow` instances built from the payload. The row class is a `TSoftClassPtr`
 * (law 3) resolved on activation -- there is no hard widget-class reference and no constructor
 * asset lookup anywhere in this file.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRModal_Options : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRModal_Options();

	/** SCREEN-MANIFEST Sec 1 and Sec 8.2: `Pop-Up Options` / `Filter Page` is 451 x 682 at x = 48. */
	static constexpr float PopupWidth = 451.0f;
	static constexpr float PopupHeight = 682.0f;
	static constexpr float PopupOriginX = 48.0f;

	/**
	 * Supply the payload. Safe before construction: values are stored and applied again from
	 * `NativeOnActivated`, where the `BindWidget` pointers are guaranteed valid.
	 */
	void SetOptionsRequest(const FBROptionsRequest& InRequest);

	/** See `UBRModal_Warning::GetDesiredInputConfig` -- a modal declares Menu input in C++, not in a WBP. */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/** First row takes focus so the gamepad lands somewhere real; CommonUI navigates from there. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_Modal_Options`. Figma layer -> UMG name:
	//   `Pop-Up Options` frame -> RootSizeBox     `Scrim`      -> Scrim
	//   `Page Title` text      -> TitleText       row list box -> RowContainer
	// `RowContainer` is a plain `UPanelWidget` so the WBP may use a vertical box today and a
	// scroll box later without a C++ change.
	// -------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRScrim> Scrim;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> RowContainer;

	/** Law 3: SOFT. Resolved on activation, never a hard `UPROPERTY` class pointer. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|UI")
	TSoftClassPtr<UBRMenuRow> RowWidgetClass;

private:
	void RebuildRows();
	void ReleaseRows();

	void HandleRowClicked(FName RowId);

	FBROptionsRequest Request;

	/**
	 * Rebuilt per activation. The modal holds at most a screenful of rows, so this is not the
	 * repeating-list case that needs pooling (`ue5-ui-architecture` Sec 5 -- that rule is about
	 * the killfeed's per-kill churn, not a list built once when a modal opens).
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRMenuRow>> RowWidgets;
};
