#pragma once

#include "Blueprint/UserWidgetPool.h"
#include "FieldNotificationId.h"
#include "UI/BRActivatableWidget.h"

#include "BRScreen_FrontEnd.generated.h"

class UBRFeatureCard;
class UBRLeftRail;
class UBRMenuRow;
class UBRNavBar;
class UBRRosterPanel;
class UBRVM_FrontEnd;
class UCommonTextBlock;
class UWidget;
class UWidgetAnimation;

/**
 * The reference file's three FE frames -- `FE_Play` (21:32824), `FE_Create` (21:32902),
 * `FE_Community` (21:32941) -- are the SAME seven instances at the SAME coordinates; only the
 * left rail's CONTENTS differ. That is why there is one screen class below and not three.
 *
 * CPP-AUDIT PKT-C cut the `BRFrontEnd` measurement namespace, `FBRFrontEndTabLayout` and
 * `ApplyTabLayout` that used to live here. `TabLayouts` was empty on every tab, so the whole
 * mechanism moved nothing -- and its layout contract was the ONLY thing requiring this screen's
 * WBP to have a root CanvasPanel, which LAYOUT-DOCTRINE Sec 6 forbids. The screen is now
 * bands-and-columns clean: geometry lives in the WBP (`MCP-BUILD-PLANS.md` Sec C2), measured
 * numbers live in COMPONENT-SPECS, and FE_Community's contradictory column-3 deltas stay
 * recorded in git history + the DECIDE ledger until a node read rules on them. If a tab ever
 * genuinely differs, that is a data-driven content widget added WITH its data -- not a dormant
 * struct waiting for some.
 */
namespace BRFrontEnd
{
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

/** Raised when the player asks for a different nav tab. The screen does NOT change tab data itself. */
DECLARE_DELEGATE_OneParam(FBRFrontEndTabChangeRequested, int32 /* TabIndex */);

/** Raised when the player commits to a rail row or the feature card. Carries the route NAME only. */
DECLARE_DELEGATE_OneParam(FBRFrontEndRouteRequested, FName /* RouteId */);

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
 * the rail's rows come from `UBRVM_FrontEnd::GetMenuRows()`, which by that ViewModel's own
 * contract holds the ACTIVE tab's rows only. Nothing else swaps — the three measured frames share
 * their geometry and their centre column exactly (the per-tab delta machinery that once lived
 * here was cut for having no data; see the header comment above). Adding a tab is a data row,
 * not a screen.
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
	void ApplyEmptyState();

	/** Everything above, in the one order that is correct. Called on activation and on any change. */
	void RefreshAll();

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_Screen_FrontEnd` (`MCP-BUILD-PLANS.md` Sec C2 is the tree):
	//   `Navigation Bar` 666x30 (header band)   -> NavBar
	//   `Menu Combo`     349x510 (column 1)     -> LeftRail
	//   `Party List`     349x273 (column 3)     -> PartyList
	//   `Progression Button` 334x115 (column 3) -> ProgressionButton
	//   (Background, Player, Profile Bar, Button Prompts and column 2 are intentionally NOT
	//    bound -- the scene reads through column 2, chrome lives in the root layout, and C++ has
	//    nothing to say to any of them.)
	//
	// NO LAYOUT CONTRACT. The old requirement that PartyList and ProgressionButton sit in a root
	// CanvasPanel died with ApplyTabLayout (CPP-AUDIT PKT-C): position is the WBP's band-and-
	// column structure now, and this screen never writes a slot.
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
	 * The 334x115 rank/record panel — a second WBP of the SAME class as the feature card
	 * (`MAIN-MENU-INVENTORY.md` Sec 4.2): a clickable ground + image + caption is exactly its
	 * shape, and the retype is what makes it gamepad-focusable at all. This screen still drives
	 * no geometry on it; once `UBRVM_Player` carries rank data, `SetCaptionText`/`SetFeatureImage`
	 * are already there to receive it.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRFeatureCard> ProgressionButton;

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

private:
	/** Weak: neither this screen nor the ViewModel owns the other. */
	TWeakObjectPtr<UBRVM_FrontEnd> BoundViewModel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRMenuRow>> MenuRowWidgets;

	/** No per-tab widget churn: rows are claimed and released, never created per tab change. */
	UPROPERTY(Transient)
	FUserWidgetPool MenuRowPool;

	UPROPERTY(Transient)
	TSubclassOf<UBRMenuRow> ResolvedMenuRowClass;

	/**
	 * `UBRNavBar::SetSelectedTabIndex` fires `OnTabSelected` for PROGRAMMATIC selection too (by
	 * design -- one path, so user and code selection cannot diverge). Without this guard, applying
	 * the ViewModel's active tab would echo straight back out as a tab-change REQUEST and loop.
	 */
	bool bApplyingViewModelState = false;

	/** Re-entry breaker on `RefreshAll` — see the comment at its definition (CPP-AUDIT D4). */
	bool bRefreshing = false;
};
