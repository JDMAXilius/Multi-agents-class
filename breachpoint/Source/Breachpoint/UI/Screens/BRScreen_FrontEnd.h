#pragma once

#include "Blueprint/UserWidgetPool.h"
#include "FieldNotificationId.h"
#include "UI/BRActivatableWidget.h"
#include "UI/Components/BRRosterPanel.h"

#include "BRScreen_FrontEnd.generated.h"

class UBRFeatureCard;
class UBRLeftRail;
class UBRMenuRow;
class UBRNavBar;
class UBRVM_FrontEnd;
class UCommonTextBlock;
class UPanelWidget;
class UUserWidget;
class UWidget;
class UWidgetAnimation;

/**
 * The measured front-end chrome, from the reference file's own three FE frames.
 *
 * `FE_Play` (21:32824), `FE_Create` (21:32902) and `FE_Community` (21:32941) are the SAME seven
 * instances at the SAME coordinates. Only the left rail's CONTENTS differ. That is why there is one
 * screen class below and not three -- see the class comment.
 *
 * A namespace rather than class statics because `FBRFrontEndTabLayout` needs these as default
 * member initialisers and the struct is declared before the class.
 */
namespace BRFrontEnd
{
	/** ui-presentation Sec 5: the design canvas every number here is measured on. */
	inline constexpr float DesignCanvasWidth = 1280.0f;
	inline constexpr float DesignCanvasHeight = 720.0f;

	/** Progression Button: 334 x 115 at (869, 55) on FE_Play and FE_Create. */
	inline constexpr float ProgressionButtonX = 869.0f;
	inline constexpr float ProgressionButtonY = 55.0f;
	inline constexpr float ProgressionButtonWidth = 334.0f;
	inline constexpr float ProgressionButtonHeight = 115.0f;

	/**
	 * FE_Community authors the Progression Button at y = **-25**, i.e. 80 px higher, which puts
	 * 25 px of a 115-tall button OFF THE TOP of the 720 canvas.
	 *
	 * READ: DRIFT, not intent. A status element cropped by the screen edge is not a design, and the
	 * companion delta below moves the OTHER right-column element the OPPOSITE way by a DIFFERENT
	 * amount (-80 here, +100 there) -- so it is not one deliberate re-composition of column 3
	 * either. Both are recorded here as named constants and NEITHER IS APPLIED: `TabLayouts` ships
	 * empty, so every tab renders the FE_Play geometry until a node read confirms otherwise. If the
	 * read says they are intentional, they become one `TabLayouts` entry and no code changes.
	 */
	inline constexpr float ProgressionButtonYCommunity = -25.0f;

	/**
	 * Party List: 349 x 273 at (862, 397). Owned by `UBRRosterPanel`; repeated here only so the
	 * per-tab override has a baseline, and static_asserted against the component below.
	 */
	inline constexpr float PartyListY = 397.0f;

	/** FE_Community authors the Party List at y = **497**: 100 lower, bottom at 770. See above. */
	inline constexpr float PartyListYCommunity = 497.0f;

	/** Player (the 3D subject): 320 x 602 at (480, 118). Invariant across all three tabs. */
	inline constexpr float PlayerStageX = 480.0f;
	inline constexpr float PlayerStageY = 118.0f;
	inline constexpr float PlayerStageWidth = 320.0f;
	inline constexpr float PlayerStageHeight = 602.0f;

	/**
	 * MOTION-MEASURED Sec 3: 150 ms per beat, measured on three reward elements entering five
	 * frames apart. A nav tab change re-populates the rail's row list, which is exactly the
	 * "any list, grid or carousel that populates" case that number was measured for.
	 *
	 * Declared, not consumed by C++: the stagger belongs in `TabSwapAnim` as a UMG timeline. This
	 * is the number that timeline must be authored to, in the one place a reviewer can diff it.
	 */
	inline constexpr float RowStaggerSeconds = 0.150f;
}

// The screen's copy of the Party List origin must agree with the component that owns it, or the
// per-tab override baseline is measuring a different panel than the one on screen.
static_assert(BRFrontEnd::PartyListY == UBRRosterPanel::PanelOriginY,
	"Front-end Party List baseline y disagrees with UBRRosterPanel::PanelOriginY.");

// RECORDED, NOT RESOLVED: the two column-3 elements do not share a right margin.
//   Party List           862 + 349 = 1211 -> 1280 - 1211 = 69  (the grid side margin)
//   Progression Button   869 + 334 = 1203 -> 1280 - 1203 = 77  (8 px inside it)
// Nothing here corrects it: a screen that "fixes" a component's measured origin is how a grid
// silently forks. It needs a node read, and it is listed as a contract_gap in this packet's report.

/** Raised when the player asks for a different nav tab. The screen does NOT change tab data itself. */
DECLARE_DELEGATE_OneParam(FBRFrontEndTabChangeRequested, int32 /* TabIndex */);

/** Raised when the player commits to a rail row or the feature card. Carries the route NAME only. */
DECLARE_DELEGATE_OneParam(FBRFrontEndRouteRequested, FName /* RouteId */);

/**
 * Per-tab layout deltas. THIS IS THE WHOLE OF WHAT VARIES GEOMETRICALLY between the three FE
 * frames, and today it is empty.
 *
 * Indexed by nav tab index, which is the same index `UBRVM_FrontEnd::GetActiveNavTabIndex` reports.
 * An index with no entry gets the FE_Play geometry, which is the correct default for all three
 * measured frames as far as anything verified says.
 */
USTRUCT(BlueprintType)
struct FBRFrontEndTabLayout
{
	GENERATED_BODY()

	/** Canvas-slot y for the Progression Button on this tab. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	float ProgressionButtonY = BRFrontEnd::ProgressionButtonY;

	/** Canvas-slot y for the Party List on this tab. X is never touched -- it is right-anchored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	float PartyListY = BRFrontEnd::PartyListY;

	/**
	 * OPTIONAL bespoke content for the centre region of this tab. Law 3: SOFT, resolved on demand,
	 * never a hard widget-class pointer.
	 *
	 * Null on every tab today, and that is the honest state: the three measured frames share their
	 * centre column exactly. The slot exists because the packet's structure calls for a swappable
	 * content region, not because a tab is known to need one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	TSoftClassPtr<UUserWidget> ContentWidgetClass;
};

/**
 * `UBRScreen_FrontEnd` -- ONE screen for `FE_Play`, `FE_Create` and `FE_Community`.
 * WBP: `WBP_Screen_FrontEnd`. Pushes to `Layer.Menu`.
 *
 * WHY ONE CLASS AND NOT THREE
 * ---------------------------
 * The three frames were read instance by instance and they are IDENTICAL: Background (0,0),
 * Navigation Bar (33,45), Menu Combo (69,138), Player (480,118), Progression Button (869,55),
 * Party List (862,397), Profile Bar (0,670), Button Prompts (60,685). The reference file's own
 * page header states the rule -- "Left third is UI, centre is the 3D subject, right is status. Left
 * rail is one Menu Combo at (69,138) 349x510" -- and SCREEN-MANIFEST Sec 4.1 states the mechanism:
 * **"Nav tabs swap the rail data, not the widget."**
 *
 * Three classes would triplicate eight instances in order to vary one, and the day the nav bar moves
 * 3 px it moves in one of the three. So the chrome is authored ONCE here and the swap is DATA:
 *   1. the rail's rows come from `UBRVM_FrontEnd::GetMenuRows()`, which by that ViewModel's own
 *      contract holds the ACTIVE tab's rows only;
 *   2. the two per-tab canvas deltas live in `TabLayouts` (empty today -- see BRFrontEnd above);
 *   3. `ContentSlot` takes an optional per-tab widget for a centre region that genuinely differs.
 * Nothing in that list is a subclass, and adding a tab is a data row, not a screen.
 *
 * WHAT THIS SCREEN DOES NOT OWN
 * -----------------------------
 * The **Profile Bar** and the **Button Prompts** are Tier 0 chrome living in `WBP_RootLayout`'s
 * chrome slot (SCREEN-MANIFEST Sec 7.1, and the class comments on `UBRProfileBar` /
 * `UBRButtonPrompt` say so in as many words: re-authoring them per screen is 31 places one blur
 * setting drifts). They are deliberately NOT bound here even though the reference file draws them
 * on all three frames. Likewise the **Background** and the **Player** stage: their geometry is
 * invariant, so C++ has nothing to say about them and they are pure WBP layout.
 *
 * INPUT
 * -----
 * `GetDesiredInputConfig` returns Menu. That override is the ONLY place input mode is declared
 * (`ue5-ui-architecture` Sec 2); nothing in this family calls `SetInputMode*`, ever. Tab cycling is
 * CommonUI's -- `UBRNavBar` registers LB/RB as input ACTIONS and puts its tabs in a
 * `UCommonButtonGroupBase`; this screen binds the resulting delegate and does no focus math and no
 * key binding of its own. Back is the activatable stack's: this screen is the stack ROOT of
 * `Layer.Menu` and so is explicitly NOT a back handler -- there is nothing beneath it to return to,
 * and a screen that faked one by toggling visibility would be the exact defect the stack exists to
 * prevent.
 *
 * STATE
 * -----
 * Reads one ViewModel and nothing else. No pawn, no ASC, no `PlayerState`, no GameState, no
 * session call, no RPC. The player's commitment to a row leaves as a route NAME through
 * `OnRouteRequested`; whoever raised this screen decides what that name means (a push, a session
 * flow, a modal). Both delegates are plain single-cast members and deliberately not `UPROPERTY`,
 * so no widget graph can reach them -- same reasoning as `FBROptionsRequest::OnRowChosen`.
 *
 * REPLICATION TIMING (`ue5-ui-architecture` Sec 7): the ViewModel may be null, may be
 * `EBRUIDataState::Unknown`, and may have zero tabs for frames after boot or travel. All three are
 * rendered honestly and differently -- see `ApplyEmptyState`.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRScreen_FrontEnd : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRScreen_FrontEnd();

	/**
	 * Push the front-end ViewModel in.
	 *
	 * CONTRACT GAP, stated rather than worked around: `UBRUIManagerSubsystem` exposes
	 * `GetCombatViewModel` and `GetMatchViewModel` and has no front-end equivalent, and nothing in
	 * the module constructs a `UBRVM_FrontEnd` yet. So the screen cannot fetch its own ViewModel the
	 * way `UBRVitalsWidget` does; it is pushed by whoever raised the screen. Safe before construct
	 * and safe with null -- `BindViewModels` re-applies on every activation.
	 */
	void SetFrontEndViewModel(UBRVM_FrontEnd* InViewModel);

	/** Set by the caller that raised this screen. Non-reflected on purpose (see the class comment). */
	FBRFrontEndTabChangeRequested OnTabChangeRequested;

	FBRFrontEndRouteRequested OnRouteRequested;

	/** `ue5-ui-architecture` Sec 2: input mode is declared HERE and in no other place. */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UUserWidget interface

	//~ Begin UCommonActivatableWidget interface
	/** The rail's own entry point, so nobody reaches into its children to guess a focus target. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget interface

	//~ Begin UBRActivatableWidget interface
	/** Called from `NativeOnActivated`. Every delegate bound here is dropped in the pair below. */
	virtual void BindViewModels() override;

	/** Called from `NativeOnDeactivated`. Nothing this screen bound may outlive its activation. */
	virtual void UnbindViewModels() override;
	//~ End UBRActivatableWidget interface

	/** Structure changed (tabs, rows, cards): rebuild. Never called for a focus move. */
	void HandleStructureChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	/** Focus changed: move a caret and a caption. Deliberately does NOT rebuild any widget. */
	void HandleFocusChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	/** `UBRNavBar::OnTabSelected` is a dynamic multicast, so this must be reflected. */
	UFUNCTION()
	void HandleNavTabSelected(int32 TabIndex);

	void HandleMenuRowClicked(FName RowId);
	void HandleMenuRowFocused(int32 RowIndex);
	void HandleFeatureCardClicked();

	void RebuildNavTabs();
	void RebuildMenuRows();
	void ReleaseMenuRows();

	void ApplyFocus();
	void ApplyFeatureCard();
	void ApplyTabLayout(int32 TabIndex);
	void ApplyContentWidget(int32 TabIndex);
	void ApplyEmptyState();

	/** Everything above, in the one order that is correct. Called on activation and on any change. */
	void RefreshAll();

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_Screen_FrontEnd`. Figma instance -> UMG name:
	//   `Navigation Bar` (33,45) 666x30      -> NavBar
	//   `Menu Combo`     (69,138) 349x510    -> LeftRail
	//   `Party List`     (862,397) 349x273   -> PartyList
	//   `Progression Button` (869,55) 334x115-> ProgressionButton
	//   the centre content region             -> ContentSlot
	//   (Background, Player, Profile Bar and Button Prompts are intentionally NOT bound -- see the
	//    class comment. Author them in the WBP / root layout and leave C++ out of them.)
	//
	// LAYOUT CONTRACT: `ProgressionButton` and `PartyList` must be direct children of the screen's
	// ROOT CanvasPanel, because `ApplyTabLayout` writes their canvas-slot y in SCREEN-LOCAL design
	// coordinates. `PartyList` keeps its RIGHT anchor (that is what makes ultrawide correct for
	// free) -- C++ writes only y and preserves whatever x the anchor produced.
	// -------------------------------------------------------------------------------------------

	/** Required: without it there is no tab spine, and this screen IS the tab spine. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRNavBar> NavBar;

	/** Required: column 1, and the gamepad's entry point into the screen. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRLeftRail> LeftRail;

	/**
	 * Optional: there is no party/roster ViewModel yet (it is BP24), so this screen can only put
	 * the panel into its honest unknown state and leave it there. It never invents a member list.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRRosterPanel> PartyList;

	/**
	 * Bound as a bare `UWidget` because no `UBRProgressionButton` component exists -- 334 x 115 is
	 * measured and unimplemented (contract_gap, components lane). This screen only ever MOVES it.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> ProgressionButton;

	/** The swappable centre region. Empty on every measured tab; see `FBRFrontEndTabLayout`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> ContentSlot;

	/** Says which KIND of nothing this is: never-told versus told-and-empty. Two different facts. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> EmptyStateLabel;

	/**
	 * The tab-change transition. Authored in the WBP, played from C++ by name only.
	 *
	 * C++ owns WHEN it plays and nothing else. MOTION-MEASURED has no measured number for a nav-tab
	 * content swap (it measures banners, a loader sting, a chip and a type-on), so this class
	 * declares no duration for it -- an invented 0.2f would be a literal in disguise. The one
	 * measured number that does apply is `BRFrontEnd::RowStaggerSeconds`.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> TabSwapAnim;

	/** Law 3: SOFT. Resolved once on initialise; never a hard widget-class `UPROPERTY`. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|UI")
	TSoftClassPtr<UBRMenuRow> MenuRowWidgetClass;

	/** Per-tab deltas, indexed by tab index. EMPTY by default, and that is the verified state. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|UI")
	TArray<FBRFrontEndTabLayout> TabLayouts;

private:
	/** Weak: neither this screen nor the ViewModel owns the other. */
	TWeakObjectPtr<UBRVM_FrontEnd> BoundViewModel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRMenuRow>> MenuRowWidgets;

	/** No per-tab widget churn: rows are claimed and released, never created per tab change. */
	UPROPERTY(Transient)
	FUserWidgetPool MenuRowPool;

	/** Created once per tab and kept, so switching back and forth does not churn widgets. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UUserWidget>> ContentWidgets;

	UPROPERTY(Transient)
	TSubclassOf<UBRMenuRow> ResolvedMenuRowClass;

	/**
	 * `UBRNavBar::SetSelectedTabIndex` fires `OnTabSelected` for PROGRAMMATIC selection too (by
	 * design -- one path, so user and code selection cannot diverge). Without this guard, applying
	 * the ViewModel's active tab would echo straight back out as a tab-change REQUEST and loop.
	 */
	bool bApplyingViewModelState = false;
};
