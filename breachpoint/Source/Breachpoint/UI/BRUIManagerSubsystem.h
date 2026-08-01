// Breachpoint. The screen-management spine: one entry point for pushing and popping screens.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"
#include "UI/BRUITypes.h"

#include "BRUIManagerSubsystem.generated.h"

class UBRActivatableWidget;
class UBRRootLayout;
class UBRVM_Combat;
class UBRVM_Match;
class ULocalPlayer;

/**
 * Everything one local player's UI owns. One of these per local player; a 4v4 online match has
 * exactly one, but modelling it as a map is what keeps splitscreen from being a rewrite and,
 * more importantly, keeps "the local player" from being an implicit global.
 */
USTRUCT()
struct FBRLocalPlayerUI
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UBRRootLayout> RootLayout = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBRVM_Combat> CombatViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBRVM_Match> MatchViewModel = nullptr;
};

/**
 * UBRUIManagerSubsystem -- BREACHPOINT-ARCHITECTURE.md 3.9, "CommonUI layer stack:
 * GameHUD/Menu/Modal".
 *
 * THE ONE SCREEN-MANAGEMENT SPINE. Screens are pushed and popped here or not at all. Nobody
 * toggles ESlateVisibility to fake navigation: a modal PUSHES over the HUD, and popping it
 * restores focus to the layer beneath automatically, which is the entire reason gamepad back
 * navigation needs no per-screen code. A hand-rolled visibility toggle skips the activation
 * tree, so the widget underneath never re-takes focus and the controller dead-ends - the exact
 * defect the M6 stranger test is designed to catch.
 *
 * ORDER OF OPERATIONS, and why it is split in two:
 *
 *   1. ViewModels are created when a LOCAL PLAYER appears (UGameInstance::OnLocalPlayerAddedEvent).
 *      They exist before any widget and before any PlayerController, so gameplay can push into
 *      them from its very first RepNotify without a null check on the UI.
 *   2. The root layout is created only when someone with a PlayerController asks
 *      (CreateLayoutForLocalPlayer), because AddToPlayerScreen needs one.
 *
 * Doing both at step 1 would mean the HUD's construction races possession; doing both at step 2
 * would mean early replicated state has nowhere to land and gets silently dropped - which shows
 * up as "the score is wrong for join-in-progress clients only".
 *
 * WHAT THIS CLASS WILL NOT DO: it has no Server RPC, touches no replicated property, and offers
 * no way for a widget to change the world. Intent goes through ABRPlayerController.
 */
UCLASS()
class BREACHPOINT_API UBRUIManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Null-safe resolve from any UObject with a world. */
	static UBRUIManagerSubsystem* Get(const UObject* WorldContextObject);

	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	// -----------------------------------------------------------------------
	// ViewModels -- the surface every other discipline pushes into.
	// -----------------------------------------------------------------------

	/**
	 * Never null for a local player the engine has told us about; null for an unknown one.
	 * Callers still check: during travel teardown the map is emptied before widgets are.
	 */
	UBRVM_Combat* GetCombatViewModel(const ULocalPlayer* LocalPlayer) const;
	UBRVM_Match* GetMatchViewModel(const ULocalPlayer* LocalPlayer) const;

	/** Convenience for the common single-local-player case. Returns the first local player's VM. */
	UBRVM_Combat* GetPrimaryCombatViewModel() const;
	UBRVM_Match* GetPrimaryMatchViewModel() const;

	// -----------------------------------------------------------------------
	// Layout lifecycle
	// -----------------------------------------------------------------------

	/**
	 * Create (or return) the root layout for a local player and add it to that player's screen.
	 *
	 * Call this from the CLIENT side of ABRPlayerController::BeginPlay (or ReceivedPlayer). It is
	 * idempotent. Returns null and logs if BRUISettings.RootLayoutClass is unset - which is the
	 * state of the project until the WBP packet lands, and is deliberately loud rather than fatal.
	 */
	UBRRootLayout* CreateLayoutForLocalPlayer(ULocalPlayer* LocalPlayer);

	/** Tear the layout down and return both ViewModels to Unknown. Travel / player removal. */
	void RemoveLayoutForLocalPlayer(ULocalPlayer* LocalPlayer);

	UBRRootLayout* GetRootLayout(const ULocalPlayer* LocalPlayer) const;

	// -----------------------------------------------------------------------
	// Screen stack
	// -----------------------------------------------------------------------

	/**
	 * Push a screen onto a layer, resolving the SOFT class first (law 3).
	 *
	 * The resolve is a synchronous load. That is a deliberate, declared cost with a declared
	 * ceiling: front-end screens are pushed at flow boundaries where a hitch is invisible, and the
	 * HUD is pushed once per match. It is NOT acceptable for anything pushed during combat, and
	 * nothing in this packet does that. If a later packet needs a mid-fight push, it needs an
	 * async handle here, not a bigger synchronous load - noted as a contract_gap.
	 */
	UBRActivatableWidget* PushWidgetToLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, const TSoftClassPtr<UBRActivatableWidget>& WidgetClass);

	/** Push an already-resolved class (bots, tests, and anything that loaded its own class). */
	UBRActivatableWidget* PushWidgetClassToLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, TSubclassOf<UBRActivatableWidget> WidgetClass);

	/**
	 * Prefer Widget->DeactivateWidget() for normal back navigation - the stack pops and restores
	 * focus by itself. This is for forced teardown.
	 */
	void RemoveWidgetFromLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, UBRActivatableWidget* Widget);

	void ClearLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag);

	// -----------------------------------------------------------------------
	// Convenience flows (thin; the front-end packet builds on these)
	// -----------------------------------------------------------------------

	/** Push BRUISettings.HUDLayoutClass onto UI.Layer.Game. */
	UBRActivatableWidget* ShowHUD(ULocalPlayer* LocalPlayer);

	/** Push BRUISettings.MainMenuScreenClass onto UI.Layer.Menu. */
	UBRActivatableWidget* ShowMainMenu(ULocalPlayer* LocalPlayer);

	/** Push BRUISettings.DeathOverlayClass onto UI.Layer.GameMenu. */
	UBRActivatableWidget* ShowDeathOverlay(ULocalPlayer* LocalPlayer);

	/** Push BRUISettings.CarnageReportClass onto UI.Layer.GameMenu. */
	UBRActivatableWidget* ShowCarnageReport(ULocalPlayer* LocalPlayer);

private:
	void HandleLocalPlayerAdded(ULocalPlayer* LocalPlayer);
	void HandleLocalPlayerRemoved(ULocalPlayer* LocalPlayer);

	FBRLocalPlayerUI& FindOrAddPlayerUI(ULocalPlayer* LocalPlayer);
	const FBRLocalPlayerUI* FindPlayerUI(const ULocalPlayer* LocalPlayer) const;

	/**
	 * Publish both ViewModels into the MVVM global collection under the configured context names,
	 * so a WBP can pick them with ContextCreationType = GlobalViewModelCollection and no code.
	 *
	 * HONEST LIMITATION: that collection is per-GameInstance, so it can only ever hold ONE
	 * player's ViewModels. Only the first local player is published. Splitscreen widgets must be
	 * handed their ViewModel explicitly (UBRHUDLayout does exactly that, via UMVVMView::SetViewModel),
	 * which is why the explicit path is the one the HUD uses and the global collection is only a
	 * convenience for front-end screens.
	 */
	void PublishViewModelsToGlobalCollection(const FBRLocalPlayerUI& PlayerUI);
	void UnpublishViewModelsFromGlobalCollection(const FBRLocalPlayerUI& PlayerUI);

	UPROPERTY(Transient)
	TMap<TObjectPtr<ULocalPlayer>, FBRLocalPlayerUI> PlayerUIs;

	/** The local player whose ViewModels are in the MVVM global collection, if any. */
	UPROPERTY(Transient)
	TObjectPtr<ULocalPlayer> PublishedLocalPlayer = nullptr;

	FDelegateHandle LocalPlayerAddedHandle;
	FDelegateHandle LocalPlayerRemovedHandle;
};
