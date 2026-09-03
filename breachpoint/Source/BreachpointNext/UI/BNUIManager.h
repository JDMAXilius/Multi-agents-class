#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"
#include "UI/BNUITypes.h"
#include "BNUIManager.generated.h"

class UBNActivatableWidget;
class UBNRootLayout;
class UBNVM_Combat;
class UBNVM_Match;
class ULocalPlayer;
struct FStreamableHandle;

/** Everything one local player's UI owns: the always-on root layout and the two ViewModels.
 *  Split-screen-correct by construction — one of these per ULocalPlayer, never a global. */
USTRUCT()
struct FBNLocalPlayerUI
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UBNRootLayout> RootLayout;

	UPROPERTY()
	TObjectPtr<UBNVM_Combat> CombatViewModel;

	UPROPERTY()
	TObjectPtr<UBNVM_Match> MatchViewModel;
};

/**
 * The UI's owner: builds the per-player root layout and ViewModels, resolves every widget class
 * from ini soft paths (C++-first — no BP child anywhere holds a widget reference), and is the
 * one push/pop door to the layer stacks. Feeding the ViewModels is NOT here — that is
 * UBNHUDDirector's, per local player; this subsystem owns objects, the director owns wiring.
 *
 * Config: [/Script/BreachpointNext.BNUIManager] in DefaultGame.ini names the five classes.
 * An unset or unbuilt path resolves null and the push logs and refuses — the designed miss
 * answer while the terminal ticket is pending, loud, never a crash.
 */
UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNUIManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** No UI machinery on a dedicated server (critic): the WBPs are stripped from a server cook,
	 *  and the preload would log streaming errors against paths that legitimately do not exist
	 *  there. The director needs no twin guard — a ULocalPlayerSubsystem never exists without a
	 *  local player. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UBNUIManager* Get(const UObject* WorldContextObject);

	/** Idempotent: builds the layout + ViewModels for this player if not built. False = the
	 *  RootLayoutClass did not resolve (unset ini or unbuilt asset) — already logged. */
	bool EnsureLocalPlayerUI(ULocalPlayer* LocalPlayer);

	UBNVM_Combat* GetCombatViewModel(ULocalPlayer* LocalPlayer) const;
	UBNVM_Match* GetMatchViewModel(ULocalPlayer* LocalPlayer) const;
	UBNRootLayout* GetRootLayout(ULocalPlayer* LocalPlayer) const;

	/** The one push door. Resolves the soft class (preloaded classes resolve without a hitch),
	 *  then pushes to the tag's stack on this player's layout. Null on any miss, loudly. */
	UBNActivatableWidget* PushWidgetToLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, const TSoftClassPtr<UBNActivatableWidget>& WidgetClass);
	void RemoveWidgetFromLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, UBNActivatableWidget* Widget);

	// The five classes, read by the director when it decides WHAT to show. Public const access
	// beats four wrapper functions.
	const TSoftClassPtr<UBNRootLayout>& GetRootLayoutClass() const { return RootLayoutClass; }
	const TSoftClassPtr<UBNActivatableWidget>& GetHUDLayoutClass() const { return HUDLayoutClass; }
	const TSoftClassPtr<UBNActivatableWidget>& GetDeathScreenClass() const { return DeathScreenClass; }
	const TSoftClassPtr<UBNActivatableWidget>& GetScoreboardClass() const { return ScoreboardClass; }
	const TSoftClassPtr<UBNActivatableWidget>& GetPostMatchScreenClass() const { return PostMatchScreenClass; }
	const TSoftClassPtr<UBNActivatableWidget>& GetPauseScreenClass() const { return PauseScreenClass; }
	const TSoftClassPtr<UBNActivatableWidget>& GetFrontEndScreenClass() const { return FrontEndScreenClass; }
	const TSoftClassPtr<UBNActivatableWidget>& GetPlaySetupScreenClass() const { return PlaySetupScreenClass; }

protected:
	void HandleLocalPlayerAdded(ULocalPlayer* LocalPlayer);
	void HandleLocalPlayerRemoved(ULocalPlayer* LocalPlayer);

	/** Death and scoreboard classes stay RESIDENT from Initialize: ShowDeathOverlay at the death
	 *  moment must never block the game thread on a sync load. The HUD itself sync-loads once at
	 *  match entry, where a hitch hides inside the load screen. */
	void PreloadMidMatchClasses();

	UPROPERTY(Config)
	TSoftClassPtr<UBNRootLayout> RootLayoutClass;

	UPROPERTY(Config)
	TSoftClassPtr<UBNActivatableWidget> HUDLayoutClass;

	UPROPERTY(Config)
	TSoftClassPtr<UBNActivatableWidget> DeathScreenClass;

	UPROPERTY(Config)
	TSoftClassPtr<UBNActivatableWidget> ScoreboardClass;

	/** BN45: the PLAYER RECAP page, pushed on WaitingPostMatch. Unset = the scoreboard is pinned instead. */
	UPROPERTY(Config)
	TSoftClassPtr<UBNActivatableWidget> PostMatchScreenClass;

	UPROPERTY(Config)
	TSoftClassPtr<UBNActivatableWidget> PauseScreenClass;

	/** The front end (M1, 1 Sep). Not in PreloadMidMatchClasses on purpose: both are
	 *  pushed from a menu map where a sync load hides inside the boot, and keeping them
	 *  resident for a whole match would be paying for a screen the match cannot show. */
	UPROPERTY(Config)
	TSoftClassPtr<UBNActivatableWidget> FrontEndScreenClass;

	UPROPERTY(Config)
	TSoftClassPtr<UBNActivatableWidget> PlaySetupScreenClass;

	UPROPERTY()
	TMap<TObjectPtr<ULocalPlayer>, FBNLocalPlayerUI> PerPlayerUI;

	TSharedPtr<FStreamableHandle> MidMatchPreloadHandle;
	FDelegateHandle PlayerAddedHandle;
	FDelegateHandle PlayerRemovedHandle;
};
