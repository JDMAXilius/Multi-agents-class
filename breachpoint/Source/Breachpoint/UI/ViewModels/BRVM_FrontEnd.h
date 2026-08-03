#pragma once

#include "MVVMViewModelBase.h"
#include "UI/BRUITypes.h"

#include "BRVM_FrontEnd.generated.h"

class UTexture2D;

/**
 * SCREEN-MANIFEST Sec 4.1: the `FE_Play` feature card (349x222) that opens an `NW_*` article.
 *
 * `TargetId` is a route NAME, not a widget class -- the screen maps it to a push. Keeping it a
 * name is what lets the carousel be table data (law 3) instead of a hard class ref (law 3 again).
 */
USTRUCT(BlueprintType)
struct FBRFeatureCardEntry
{
	GENERATED_BODY()

	/** SOFT (law 3). Null = art not resolved yet; the card shows its plate, not a broken image. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	TSoftObjectPtr<UTexture2D> Image;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText Body;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FName TargetId;
};

/**
 * SCREEN-MANIFEST Sec 4.1: one row of the Left Rail's Menu Combo (`UBRMenuRow`, h28 pitch 40).
 *
 * `Description` lives on the row, not on the ViewModel, because the Description Strip shows the
 * FOCUSED row's text -- one stored focus index derives it (see `GetFocusedRowDescription`).
 *
 * No row `Type` here on purpose: the visual type axis is `EBRMenuRowType` on the widget
 * (`UI/Components/BRMenuRow.h`) and belongs to the screen's layout, not to this data. A ViewModel
 * that names widget variants has the dependency arrow pointing the wrong way.
 */
USTRUCT(BlueprintType)
struct FBRMenuRowEntry
{
	GENERATED_BODY()

	/** Stable routing key (`MM_Root`, `OP_Customize`, `Quit`...). The screen owns the mapping. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FName RowId;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText Label;

	/** Shown in the 349x37 Description Strip while this row holds focus. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText Description;

	/** False = present but not selectable (feature not unlocked / service down). */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	bool bEnabled = true;
};

/**
 * `FE_Splash` / `FE_Splash_Seasonal` prompt state. `Unknown` is the boot value: the splash does
 * not know whether input is being accepted until the boot flow says so, and a prompt that invites
 * a press the game will ignore is worse than no prompt.
 */
UENUM(BlueprintType)
enum class EBRPressToStartState : uint8
{
	Unknown,

	/** Deliberately suppressed (boot movie, transition). */
	Hidden,

	/** Prompt visible, input accepted. */
	Awaiting,

	/** Pressed; sign-in / load underway. Prompt shows busy, further presses are ignored. */
	Acknowledged
};

/**
 * `UBRVM_FrontEnd` -- SCREEN-MANIFEST Sec 10 gap G2 (blocks 4 screens, gates Wave 1).
 *
 * The front-end shell's own data: the feature-card carousel, the left rail's rows for whichever
 * nav tab is active, the focused-row description, the splash's season art and prompt state, and
 * the one responsive flag from Sec 8.3.
 *
 * SEC 4.1, THE LOAD-BEARING SENTENCE: "Nav tabs swap the rail data, not the widget." So this
 * holds the rows for the ACTIVE tab only. The per-tab menu definition is table data owned by a
 * future front-end subsystem (law 3), which calls `SetNavTab(Index, Rows)` on a tab change. A map
 * of every tab's rows cached here would be a second copy of a CSV.
 *
 * NO PRODUCER EXISTS YET. Nothing here reads the world; every field boots to an explicit unknown
 * or an empty list, and empty is an honest front end (no cards, no rows) rather than a fabricated
 * one. No Tick, no polling (law 4).
 */
UCLASS(BlueprintType, DisplayName = "BR Front End Viewmodel")
class BREACHPOINT_API UBRVM_FrontEnd : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:

	// -------------------------------------------------------------------------------------
	// Feeds.
	// -------------------------------------------------------------------------------------

	/** Replaces the carousel wholesale; resets focus to the first card, or to none if empty. */
	void SetFeatureCards(const TArray<FBRFeatureCardEntry>& InCards);

	/** Carousel focus, driven by nav/bumper input. Out-of-range clamps to `INDEX_NONE`. */
	void SetFocusedFeatureCardIndex(int32 InIndex);

	void SetNavTabLabels(const TArray<FText>& InLabels);

	/**
	 * One call, because the tab index and its rows are one atomic truth: setting them separately
	 * paints tab 2's rows under tab 1's highlight for a frame. Focus resets to the first row.
	 */
	void SetNavTab(int32 InTabIndex, const TArray<FBRMenuRowEntry>& InRows);

	/** Rail focus, driven by gamepad navigation. This is what moves the Description Strip. */
	void SetFocusedMenuRowIndex(int32 InIndex);

	void SetSeasonKeyArt(const TSoftObjectPtr<UTexture2D>& InKeyArt);

	void SetPressToStartState(EBRPressToStartState InState);

	/**
	 * SCREEN-MANIFEST Sec 8.3: below 1280 design px of width the status band (column 3) stops
	 * being a persistent panel and becomes a shoulder-summoned overlay. Fed by whoever observes
	 * the viewport; it is a local layout fact, never replicated.
	 */
	void SetStatusBandPersistent(bool bInPersistent);

	void MarkStale();

	void ClearToUnknown();

	// -------------------------------------------------------------------------------------
	// Reads.
	// -------------------------------------------------------------------------------------

	EBRUIDataState GetMenuState() const { return MenuState; }

	const TArray<FBRFeatureCardEntry>& GetFeatureCards() const { return FeatureCards; }
	int32 GetFocusedFeatureCardIndex() const { return FocusedFeatureCardIndex; }

	const TArray<FText>& GetNavTabLabels() const { return NavTabLabels; }
	int32 GetActiveNavTabIndex() const { return ActiveNavTabIndex; }
	const TArray<FBRMenuRowEntry>& GetMenuRows() const { return MenuRows; }
	int32 GetFocusedMenuRowIndex() const { return FocusedMenuRowIndex; }

	/**
	 * DERIVED from the focus index + the row list -- the Description Strip's only binding. Stored
	 * separately it would drift out of step with the row the player is actually on.
	 *
	 * Empty when nothing is focused, which the strip renders as an empty strip. It never collapses
	 * layout (a strip that appears and disappears makes the rail jump under the cursor).
	 */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Breachpoint|FrontEnd")
	FText GetFocusedRowDescription() const;

	TSoftObjectPtr<UTexture2D> GetSeasonKeyArt() const { return SeasonKeyArt; }
	EBRPressToStartState GetPressToStartState() const { return PressToStartState; }
	bool IsStatusBandPersistent() const { return bStatusBandPersistent; }

private:

	/** Covers the fed content (cards + rows + season art). Boots Unknown: nothing has fed yet. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMenuState", Category = "Breachpoint|FrontEnd", meta = (AllowPrivateAccess))
	EBRUIDataState MenuState = EBRUIDataState::Unknown;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetFeatureCards", Category = "Breachpoint|FrontEnd", meta = (AllowPrivateAccess))
	TArray<FBRFeatureCardEntry> FeatureCards;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetFocusedFeatureCardIndex", Category = "Breachpoint|FrontEnd", meta = (AllowPrivateAccess))
	int32 FocusedFeatureCardIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetNavTabLabels", Category = "Breachpoint|FrontEnd", meta = (AllowPrivateAccess))
	TArray<FText> NavTabLabels;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetActiveNavTabIndex", Category = "Breachpoint|FrontEnd", meta = (AllowPrivateAccess))
	int32 ActiveNavTabIndex = INDEX_NONE;

	/** The rows of the ACTIVE tab only -- Sec 4.1: the tab swaps the data, not the widget. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMenuRows", Category = "Breachpoint|FrontEnd", meta = (AllowPrivateAccess))
	TArray<FBRMenuRowEntry> MenuRows;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetFocusedMenuRowIndex", Category = "Breachpoint|FrontEnd", meta = (AllowPrivateAccess))
	int32 FocusedMenuRowIndex = INDEX_NONE;

	/** SOFT (law 3). `FE_Splash_Seasonal` is this field changing, not a second widget. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetSeasonKeyArt", Category = "Breachpoint|FrontEnd", meta = (AllowPrivateAccess))
	TSoftObjectPtr<UTexture2D> SeasonKeyArt;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetPressToStartState", Category = "Breachpoint|FrontEnd", meta = (AllowPrivateAccess))
	EBRPressToStartState PressToStartState = EBRPressToStartState::Unknown;

	/**
	 * Sec 8.3 / Sec 8.1. The one bool here with no Unknown state, and deliberately so: it is
	 * derived from the local viewport, which is known on the first frame and never replicated, so
	 * "not yet known" cannot occur. It defaults to the 16:9 design target (persistent).
	 */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "IsStatusBandPersistent", Category = "Breachpoint|FrontEnd", meta = (AllowPrivateAccess))
	bool bStatusBandPersistent = true;
};
