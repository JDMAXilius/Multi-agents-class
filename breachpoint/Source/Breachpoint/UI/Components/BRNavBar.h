#pragma once

#include "CommonButtonBase.h"
#include "CommonUserWidget.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRNavBar.generated.h"

class UBRButtonPrompt;
class UBRHairlineBorder;
class UCommonButtonGroupBase;
class UCommonTextBlock;
class UImage;
class UInputAction;
class UPanelWidget;
class USizeBox;
class UTexture2D;

/**
 * SCREEN-MANIFEST Sec 6.1 "Variant axis": `Menu = Template | Main | Start Menu | Roster Menu`.
 *
 * The axis is DECLARED, not implemented as geometry: tab COUNT varies with `Menu`, but the tab
 * set itself is data (see `SetTabs`), so this enum drives no layout. It exists so a screen can
 * say which of the four nav bars it is, and so a reviewer can see the fourth value is not
 * missing. Inventing a per-value tab list here would fork the front-end menu definition.
 */
UENUM(BlueprintType)
enum class EBRNavBarMenu : uint8
{
	Template,

	Main,

	StartMenu,

	RosterMenu
};

/** One tab's data. Soft icon ref only (law 3); no hard asset pointer reaches this header. */
USTRUCT(BlueprintType)
struct FBRNavTabDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	FText Label;

	/** COMPONENT-SPECS Sec 3 / SCREEN-MANIFEST Sec 6.1: the 24x24 icon is OPTIONAL and unset here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * `UBRNavTab` -- one 138 x 26 tab, the entry widget of `UBRNavBar`.
 *
 * BASE: `UCommonButtonBase` (which IS a `UCommonUserWidget`, so there is still one widget base),
 * for the same reason `UBRMenuRow` uses it: CommonUI already owns focus, hover, navigation and
 * gamepad routing. A tab strip that hand-rolls "which one is focused" is the classic mouse-only
 * path and a gamepad finding (`ue5-ui-architecture` Sec 6). Selection exclusivity is likewise not
 * hand-rolled -- `UBRNavBar` puts every tab in a `UCommonButtonGroupBase`.
 *
 * COMPONENT-SPECS Sec 3 is the whole visual: a CLOSED rectangle border (unlike `UBRMenuRow`'s
 * four partial lines), and `Active=False` dims the WHOLE tab to 0.6 rather than restyling it.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRNavTab : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** COMPONENT-SPECS Sec 3 / SCREEN-MANIFEST Sec 6.1: COMPONENT 138 x 26. */
	static constexpr float TabWidth = 138.0f;
	static constexpr float TabHeight = 26.0f;

	/** COMPONENT-SPECS Sec 3: Icon 24 x 24, INSTANCE_SWAP, optional. */
	static constexpr float TabIconSize = 24.0f;

	/**
	 * COMPONENT-SPECS Sec 3: weight 3 when Active. Drawn as `EBRStrokeWeight::Focus` — the 3px
	 * token that closed the old "not expressible" gap. The measured stroke ALIGNMENT is OUTSIDE,
	 * which the hairline leaf does not model; the 0.6 opacity below carries the state read and is
	 * part of the spec, not a substitute. Text/icon geometry lives in COMPONENT-SPECS Sec 3 and
	 * the WBP plan — CPP-AUDIT cut the constants that mirrored it here unread.
	 */
	static constexpr float InactiveOpacity = 0.6f;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetTabLabel(const FText& InText);

	/**
	 * Soft ref in, resolved here. An unset icon COLLAPSES the slot rather than drawing an empty
	 * 24x24 hole -- the icon is optional in the reference and a blank box would read as a
	 * missing asset.
	 */
	void SetTabIcon(const TSoftObjectPtr<UTexture2D>& InIcon);

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	//~ Begin UCommonButtonBase interface
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	//~ End UCommonButtonBase interface

	/** The single place Active/Inactive is expressed. */
	void ApplyActiveState(bool bActive);

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_NavTab`. Figma layer -> UMG name:
	//   `Border` -> Border      `Text` -> Label      `Icon` -> Icon
	// ---------------------------------------------------------------------------------------

	/** C++ drives 138 x 26 so four Menu variants cannot drift to four sizes. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/** COMPONENT-SPECS Sec 3: a full RECTANGLE, all four edges, none dimmed. Not a menu row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> Border;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken StrokeToken = EBRUIColorToken::ChromeStroke;

private:
	bool bIsActive = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBRNavBarTabSelected, int32, TabIndex);

/**
 * `UBRNavBar` -- the navigation bar (SCREEN-MANIFEST Sec 5 Tier 1: 18 of 31 screens; the
 * highest-leverage piece of chrome in the front end).
 *
 * GEOMETRY, and the number that was in dispute:
 * COMPONENT-SPECS Sec 3 / Sec 6 and SCREEN-MANIFEST Sec 6.1 measure the bar at 666 x 30 at
 * **(33, 45)** on live node `124:1179` of `Kn87U5sy2VD0lP8K7h4LcQ`, re-confirmed 2 Aug 2026.
 * SCREEN-MANIFEST Sec 9 question 1 is RESOLVED in favour of **x = 33**; `SCREEN-BUILD-SPEC` Sec 1's
 * `44,45` is a recorded defect (TICKET_BP67). 18 screens inherit this number, so any screen
 * already authored at x=44 is wrong and must be re-checked -- that is why the origin is a named
 * constant here instead of a number typed into 18 details panels.
 *
 * BASE: `UCommonUserWidget`, not `UBRActivatableWidget`. The nav bar is chrome inside a screen;
 * the SCREEN is the activatable that pushes and pops (`ue5-ui-architecture` Sec 1-2). Being
 * activatable would put a second entry on the stack for a widget that owns no back behaviour.
 * Its bumper bindings are still activation-scoped: `RegisterUIActionBinding` on a plain
 * `UCommonUserWidget` only listens while every activatable parent is active, which is exactly
 * the desired "LB/RB cycle tabs only on the front screen" behaviour.
 *
 * INPUT: the bumpers are CommonUI input ACTIONS (`FBindUIActionArgs`), never raw keys
 * (`ue5-ui-architecture` Sec 6), and the on-screen glyphs are `UBRButtonPrompt` -- the Tier-0
 * component that already carries the reference's `Bumper L` / `Bumper R` variants -- so the
 * platform icon swaps itself with the input device.
 *
 * STATE: the bar holds no authoritative state and calls no RPC. It renders tab definitions it is
 * given and broadcasts an INDEX; routing that index to a screen change is the owner screen's job.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRNavBar : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** COMPONENT-SPECS Sec 3 / Sec 6, node 124:1179: COMPONENT 666 x 30 at (33, 45). */
	static constexpr float BarWidth = 666.0f;
	static constexpr float BarHeight = 30.0f;

	/**
	 * The sub-level bar. Width is quoted twice and consistently (516 x 30); its ORIGIN is
	 * unverified and deliberately NOT declared -- the candidate y values live on `UBRItemTitle`
	 * where the swap that causes them is explained. Do not add sub-level geometry without a
	 * node read (SCREEN-MANIFEST Sec 9 Q1).
	 */
	static constexpr float SubLevelBarWidth = 516.0f;

	/** COMPONENT-SPECS Sec 3: tabs at x = 39, 189, 339, 489 -- PITCH 150 on a 138-wide tab. */
	static constexpr float TabPitch = 150.0f;

	/**
	 * Pitch 150 on a 138 tab means a 12 px inter-tab gap. Applied by `SetTabs` as slot padding
	 * on every tab after the first (SCREEN-MANIFEST Sec 7.1: a Figma gap is slot padding, never
	 * a spacer widget) -- the WBP ships an EMPTY container, so the gap must live where the tabs
	 * are created. Bumper geometry (27 x 15) and its origin ambiguity moved to
	 * `MCP-BUILD-PLANS.md` with the rest of the WBP measurements.
	 */
	static constexpr float TabGap = TabPitch - UBRNavTab::TabWidth;

	/**
	 * Replace the tab set. Rebuilds the entries -- called on screen setup and when the menu
	 * definition changes, NEVER per frame (`ue5-ui-architecture` Sec 5: no widget churn in a
	 * repeating path). An EMPTY array collapses the whole bar: front-end data can be absent for
	 * frames after travel, and an empty 666x30 outline reads as a broken screen rather than an
	 * honest "no tabs yet".
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetTabs(const TArray<FBRNavTabDefinition>& InTabs);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetSelectedTabIndex(int32 InIndex);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	int32 GetSelectedTabIndex() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	int32 GetTabCount() const { return Tabs.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetMenuVariant(EBRNavBarMenu InMenu) { Menu = InMenu; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRNavBarMenu GetMenuVariant() const { return Menu; }

	/** Fires on user selection AND on programmatic selection -- one path, so they cannot diverge. */
	UPROPERTY(BlueprintAssignable, Category = "Breachpoint|UI")
	FBRNavBarTabSelected OnTabSelected;

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	/** Lazily built: `SetTabs` may legitimately arrive before the first construct. */
	UCommonButtonGroupBase* GetOrCreateTabGroup();

	void RegisterBumperActions();

	void HandleSelectPreviousTab();
	void HandleSelectNextTab();
	void HandleTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 SelectedIndex);

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_NavBar`. Figma layer -> UMG name:
	//   the 666x30 component root -> RootSizeBox
	//   the tab row              -> TabContainer   (a UHorizontalBox; gap TabGap = 12)
	//   `Bumper L` / `Bumper R`  -> BumperPrev / BumperNext
	// ---------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> TabContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRButtonPrompt> BumperPrev;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRButtonPrompt> BumperNext;

	/** Soft (law 3). The entry class is a WBP; C++ never hard-references one. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|UI")
	TSoftClassPtr<UBRNavTab> TabWidgetClass;

	/** CommonUI input actions, soft. The UI never binds a raw key. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	TSoftObjectPtr<UInputAction> PreviousTabAction;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	TSoftObjectPtr<UInputAction> NextTabAction;

	/** Empty by default: the reference's bumper prompts are glyph-only, and an English literal
	 *  in C++ is a localization defect. A screen may supply a verb. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	FText PreviousTabVerb;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	FText NextTabVerb;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRNavBarMenu Menu = EBRNavBarMenu::Template;

	/** Selects between the two MEASURED widths (666 root, 516 sub-level). Nothing else. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	bool bSubLevel = false;

	/** Wrapping is what makes LB/RB usable with a thumb and no mouse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|Input")
	bool bBumpersWrap = true;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRNavTab>> Tabs;

	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonGroupBase> TabGroup;

	/**
	 * CPP-AUDIT D5. `Group->AddWidget` under selection-required auto-selects the first tab, and
	 * that broadcast can route through the owning screen straight back into `SetTabs` mid-loop —
	 * which would clear `Tabs` and the container while the outer loop keeps appending orphans.
	 * While true: re-entrant `SetTabs` calls are dropped and selection broadcasts are held; the
	 * one real selection is broadcast once, after the rebuild completes.
	 */
	bool bRebuildingTabs = false;
};
