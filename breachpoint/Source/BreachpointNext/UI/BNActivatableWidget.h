#pragma once

#include "CommonActivatableWidget.h"
#include "CommonInputModeTypes.h"
#include "UI/BNUITypes.h"
#include "BNActivatableWidget.generated.h"

class UBNVM_Combat;
class UBNVM_Match;

/** What input a screen wants while it is on top. Declared per CLASS DEFAULT, honored by
 *  CommonUI's action router through GetDesiredInputConfig — the one lawful way input modes
 *  change. Nothing anywhere calls SetInputMode* by hand. */
UENUM()
enum class EBNWidgetInputMode : uint8
{
	/** Take whatever the widget below already established. */
	Inherit,
	/** The game plays under this widget: HUD. Mouse captured. */
	Game,
	/** Game keeps running but menus are usable: pause-adjacent screens. */
	GameAndMenu,
	/** Pure menu: the game gets no input. */
	Menu
};

/**
 * THE one widget base for everything pushed to a layer. There is no second base — a screen that
 * can't derive this is a design question, not a new hierarchy.
 *
 * Bind on activate, unbind on deactivate: BindViewModels/UnbindViewModels are the only places a
 * screen touches a ViewModel delegate, and a binding that survives NativeOnDeactivated is the
 * crash the death screen finds. HUD SURFACES (match band, killfeed…) deliberately do NOT derive
 * this — they are never pushed to a layer, so activation scope is construct scope, and the
 * activatable base costs bSupportsActivationFocus, a focus hazard the old module paid to learn.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/** Subclass seams, called from the two lifecycle hooks above. Subscribe here, then read the
	 *  current state ONCE — the delegate contract every BN feed documents. */
	virtual void BindViewModels() {}
	virtual void UnbindViewModels() {}

	/** The owning local player's ViewModels, from the manager. Null before the manager built
	 *  them — a subclass renders honest-unknown on null, never assumes. */
	UBNVM_Combat* GetCombatViewModel() const;
	UBNVM_Match* GetMatchViewModel() const;

	UPROPERTY(EditDefaultsOnly, Category = "BN|Input")
	EBNWidgetInputMode InputMode = EBNWidgetInputMode::Inherit;

	UPROPERTY(EditDefaultsOnly, Category = "BN|Input")
	bool bHideCursorDuringViewportCapture = true;
};
