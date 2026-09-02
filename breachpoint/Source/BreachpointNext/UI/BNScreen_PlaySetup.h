#pragma once

#include "UI/BNSettingsPanel.h"

#include "UI/BNActivatableWidget.h"
#include "BNScreen_PlaySetup.generated.h"

class UBNProfileBar;
class UBNPromptButton;
class UBNTeamRoster;
class UBRHighlightButton;
class UBRButton;
class UBRPageTitle;
class UImage;
class UTextBlock;
class UTexture2D;

/**
 * One playable map on the front end. DATA, not code (law 3): the roster lives in
 * DefaultGame.ini under [/Script/BreachpointNext.BNScreen_PlaySetup], so adding a map to
 * the menu is an ini line, never a compile. Paths are strings on purpose — the map is
 * opened by name at travel time, not held as a reference the cooker chases.
 */
USTRUCT()
struct FBNFrontEndMapEntry
{
	GENERATED_BODY()

	/** As printed on the row and the breakdown panel — "SPILLWAY", not the asset name. */
	UPROPERTY(Config)
	FString DisplayName;

	/** The hint-strip line while this map is selected. */
	UPROPERTY(Config)
	FString Description;

	/** Long package name, e.g. /Game/Maps/BR_Spillway. */
	UPROPERTY(Config)
	FString MapPath;

	/** The 349x196.7 preview plate for this map. SOFT (law 3): the front end is the only
	 *  thing that ever loads it, and it must not drag a map's art into every cook. Empty
	 *  is legal and means "no plate" — the Image collapses rather than showing a white box. */
	UPROPERTY(Config)
	TSoftObjectPtr<UTexture2D> PreviewTexture;

	/**
	 * The DETAILS panel's rows for this map — LABEL and VALUE, not one packed string.
	 *
	 * WAS `TArray<FString>` of free-form lines padded with spaces ("SPAWNS        16"), which
	 * only lined up because a monospaced-ish font and hand-counted padding happened to agree.
	 * Node `21:43050` right-ALIGNS the value to the panel's 349 edge, so the two halves are two
	 * fields; packing them into one string cannot reproduce that at any label width, and the
	 * founder asked for details that space themselves correctly.
	 *
	 * Empty is legal — the section collapses rather than printing an empty rule.
	 */
	UPROPERTY(Config)
	TArray<FBNSettingRow> Details;
};

/**
 * PLAY SETUP — the custom-game lobby, single-machine edition (founder, 1 Sep: "you just go
 * to play, and then you select the map, you select if you want free-for-all or team
 * deathmatch, and you select the numbers of AI bots").
 *
 * Figma truth: `01-MENU-MEASURED.md` §4 — CG_Lobby `21:43019`. Left rail Menu Combo at
 * (69,76) with rows MAP / MODE / BOTS / START GAME; centre Game Settings Breakdown at
 * (466,76) mirrors the current selection. The roster column ships in a later phase — with
 * no sessions there is nobody to list but bots that do not exist until the match does.
 *
 * SELECTORS CYCLE. A row click steps to the next value and wraps — the reference UI's own
 * row-with-value-on-the-right pattern, and it needs no popup chassis, which keeps the
 * first playable menu to two screens. The popup (451×682, measured) is the M2 upgrade.
 *
 * THE LAUNCH IS A URL. Start composes ?TargetPlayers=N?Teams=0|1 and opens the level;
 * ABNGameMode::InitGame parses those and everything downstream — bot fill to TargetPlayers,
 * team assignment, HUD — already exists and is already proven in matches. This screen
 * invents no match machinery; it only parameterises the machinery the game has.
 */
UCLASS(Abstract, Config = Game, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNScreen_PlaySetup : public UBNActivatableWidget
{
	GENERATED_BODY()

public:
	UBNScreen_PlaySetup();

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleMapClicked();

	UFUNCTION()
	void HandleModeClicked();

	UFUNCTION()
	void HandleBotsClicked();

	UFUNCTION()
	void HandleScoreLimitClicked();

	UFUNCTION()
	void HandleTimeLimitClicked();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleBackClicked();

	/** Push every current value into the bound texts. One writer, called after any step,
	 *  so the rail and the breakdown panel can never disagree. */
	void RefreshDisplay();

	/** The founder's defaults: FFA is a full lobby of eight (you + seven bots); teams is
	 *  4v4. Applied when the MODE changes, then the BOTS row edits freely from there. */
	int32 DefaultPlayersForMode(bool bForTeams) const { return 8; }

	// -- the three selectors -----------------------------------------------------------

	/**
	 * The three cyclers and START, as the MEASURED Menu Row component (Figma `12:724`).
	 *
	 * UBRButton carries BOTH halves of a settings row — SetLabelText on the left, and
	 * SetSelectionText on the right — which is exactly the label/value split this screen
	 * used to fake with an Overlay holding two CommonTextBlocks. Those are gone: the row
	 * is one widget again, and the value can never drift out of alignment with its label.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UBRButton> MapButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UBRButton> ModeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UBRButton> BotsButton;

	/** SCORE LIMIT / TIME LIMIT (founder, 2 Sep 2026). Same row, same cycle-on-click. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UBRButton> ScoreLimitButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UBRButton> TimeLimitButton;

	/** `Button Prompts` `21:43024` as controls: both return to the front end. Optional so the
	 *  front end's own copy of the legend (which has no "back") can share the class. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBNPromptButton> BackPrompt;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBNPromptButton> MenuPrompt;

	/**
	 * START GAME is the reference's `Action Button`, not a fourth menu row — the affirmative
	 * action on the screen, and the one row the design treats differently. `UBRHighlightButton`
	 * is that class; it is a `UCommonButtonBase` like `UBRButton`, so `OnClicked()` is
	 * unchanged, but it carries the amber/inverted highlight treatment instead of the row
	 * inversion. It has no `SetSelectionText`, which is fine — this row prints no value.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UBRHighlightButton> StartButton;


	// -- the values those rows print, and the breakdown panel's mirrors ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UTextBlock> DescriptionText;

	/** The map plate. Optional so a WBP without one still compiles and still plays. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UImage> PreviewImage;

	/**
	 * The centre column, `Game Settings Breakdown` `21:43050` — one measured component where a
	 * Border + a "DETAILS" label + one packed TextBlock used to be. It owns the gamemode card
	 * and lays its own rows out at the measured pitch 23, so a map with three details and a map
	 * with nine both space correctly without touching the WBP.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBNSettingsPanel> SettingsPanel;

	/** The 1280x75 band, `Page Title` `21:43048`. Reused, not re-authored. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBRPageTitle> PageTitle;

	/** The footer strip — the SAME component the front end uses. Modularity is the point. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBNProfileBar> ProfileBar;

	/** The third column, `Menu in Border` `21:43056` at (863,38) 349 x 599. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBNTeamRoster> TeamRoster;

	/**
	 * The gamemode icon shown on the settings card. SOFT (law 3) and set from ini, so the
	 * mode's art is data — a new mode is an ini line, not a compile.
	 */
	UPROPERTY(Config)
	TSoftObjectPtr<UTexture2D> TeamsModeIcon;

	UPROPERTY(Config)
	TSoftObjectPtr<UTexture2D> FreeForAllModeIcon;

	/**
	 * Who the lobby lists. Config, not literals in the .uasset — the same rule the front end's
	 * roster follows. Entry 0 is the local player. Names beyond the selected player count are
	 * simply not shown, so one list serves 4v4 and 8v8 without a second array.
	 */
	UPROPERTY(Config)
	TArray<FString> LobbyPlayerNames;

	/** One per team, in order. Two entries = two teams; FFA uses only the first. */
	UPROPERTY(Config)
	TArray<FLinearColor> TeamColors;

	/** One per team, in order — COBRA, EAGLE. Parallel to TeamColors. */
	UPROPERTY(Config)
	TArray<FString> TeamNames;

	/** Shared roster emblem. SOFT (law 3); a FRAME, not a portrait. */
	UPROPERTY(Config)
	TSoftObjectPtr<UTexture2D> RosterEmblem;

	// -- data + state ------------------------------------------------------------------

	/** The menu's map roster, from ini. Empty = the designed miss: START logs and refuses,
	 *  the rows print the miss, nothing crashes. */
	UPROPERTY(Config)
	TArray<FBNFrontEndMapEntry> Maps;

	/** Lobby-size presets the BOTS row cycles. Total INCLUDING the human — the founder
	 *  counts the player ("eight bots in total counting the player"). */
	UPROPERTY(Config)
	TArray<int32> PlayerCountPresets = { 4, 8, 12, 16 };

	/** Kills to win. Founder's list; ini-editable like the player counts. */
	UPROPERTY(Config)
	TArray<int32> ScoreLimitPresets = { 10, 20, 30 };

	/** Minutes on the clock. Travels as seconds (`?TimeLimit=`), shown as minutes. */
	UPROPERTY(Config)
	TArray<int32> TimeLimitPresets = { 5, 10, 15, 20 };

	/**
	 * TEAM DEATHMATCH is what the menu opens on (founder, 1 Sep: "team deathmatch should be
	 * default on mainmenu"). Config rather than a literal so the default is an ini line, and
	 * because a headless test wanting the FFA path should not have to fake a click.
	 */
	UPROPERTY(Config)
	bool bDefaultTeams = true;

	int32 MapIndex = 0;
	bool bTeams = true;
	int32 TotalPlayers = 8;
	int32 ScoreLimitIndex = 0;
	int32 TimeLimitIndex = 1;   // 10 minutes — the ini's 600s default, so the URL and a plain boot agree
};
