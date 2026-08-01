// Breachpoint. The one widget base. Every screen and panel derives from this.

#pragma once

#include "CommonActivatableWidget.h"
#include "CommonInputModeTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Input/UIActionBindingHandle.h"
#include "UI/BRUITypes.h"

#include "BRActivatableWidget.generated.h"

class UBRVM_Combat;
class UBRVM_Match;
class UBRUIManagerSubsystem;

/**
 * How this widget wants input routed while it is the leaf-most active widget.
 *
 * SKILL CORRECTION (ue5-ui-architecture 2 said "Game, GameAndMenu, Menu"): UE 5.8's
 * ECommonInputMode (CommonInput/Public/CommonInputModeTypes.h) has exactly three enumerators -
 * Menu, Game, All. There is NO `GameAndMenu`; writing it is a compile error. The mapping below
 * is the whole correction, kept as a named project enum so the intent reads at the call site.
 */
UENUM(BlueprintType)
enum class EBRWidgetInputMode : uint8
{
	/**
	 * Declare nothing. GetDesiredInputConfig() returns an unset TOptional and CommonUI keeps
	 * whatever the widget below established. This is the correct default for panels that are
	 * not screens (an ammo block must not seize the input config).
	 */
	Inherit,

	/** ECommonInputMode::Game. The HUD. Mouse captured, cursor hidden. */
	Game,

	/** ECommonInputMode::All. In-match overlays: the player is still being shot at. */
	GameAndMenu,

	/** ECommonInputMode::Menu. Front end, modals. Cursor shown, nothing reaches the pawn. */
	Menu
};

/**
 * UBRActivatableWidget -- BREACHPOINT-ARCHITECTURE.md 3.9, "the one widget base".
 *
 * There is no second widget base. Screens, panels, and the HUD all derive from here, which is
 * what makes CommonUI's activation tree - and therefore gamepad focus, back-handling, and
 * input-mode hand-off - a single mechanism instead of per-screen bespoke code.
 *
 * THE THREE RULES THIS CLASS ENFORCES
 *
 * 1. No tick. UCommonActivatableWidget is already UCLASS(meta = (DisableNativeTick)) in UE 5.8
 *    and this class re-states it. A HUD that polls the ASC every frame is both a law-4
 *    violation and, at 8 players x N widgets, a real frame cost.
 *
 * 2. Input mode is DECLARED, never set. Override GetDesiredInputConfig() (or just set
 *    InputMode) - never call SetInputMode* from a widget or a controller. CommonUI applies the
 *    leaf-most active widget's config and RESTORES the previous one on deactivation; a manual
 *    SetInputMode call desynchronises that stack and the symptom shows up two screens later.
 *
 * 3. Bind on activate, unbind on deactivate, symmetrically. Subclasses override
 *    BindViewModels()/UnbindViewModels() and NOTHING else for lifecycle. A ViewModel delegate
 *    that outlives its widget is the crash the death overlay finds, because the death overlay
 *    is the first screen that gets torn down while its feed is still broadcasting.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	//~ UCommonActivatableWidget
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~ End UCommonActivatableWidget

	/**
	 * The ViewModels for THIS widget's owning local player.
	 *
	 * Both may be null and callers must behave. Null is not an error state - it is the normal
	 * situation between AddToPlayerScreen and the local player's subsystem being warmed up, and
	 * on a dedicated-server-hosted client the frame after seamless travel.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	UBRVM_Combat* GetCombatViewModel() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	UBRVM_Match* GetMatchViewModel() const;

protected:
	//~ UCommonActivatableWidget
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	//~ End UCommonActivatableWidget

	/**
	 * Subscribe to ViewModel fields here. Called from NativeOnActivated AFTER the base class has
	 * run, so GetCombatViewModel()/GetMatchViewModel() are as valid as they are ever going to be.
	 *
	 * Contract: everything bound here is unbound in UnbindViewModels(). The pair is symmetric or
	 * it is a finding.
	 */
	virtual void BindViewModels() {}

	/** Unsubscribe everything BindViewModels() subscribed to. Called from NativeOnDeactivated. */
	virtual void UnbindViewModels() {}

	/** Resolve the UI manager for this widget's owning local player. May be null during teardown. */
	UBRUIManagerSubsystem* GetUIManager() const;

	/**
	 * Declared input routing. Kept EditDefaultsOnly so a WBP subclass can state it without a
	 * graph node - that is layout-adjacent configuration, not gameplay logic.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	EBRWidgetInputMode InputMode = EBRWidgetInputMode::Inherit;

	/**
	 * Whether the cursor is hidden while the viewport has capture. Only consulted for
	 * EBRWidgetInputMode::Game / GameAndMenu.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	bool bHideCursorDuringViewportCapture = true;
};
