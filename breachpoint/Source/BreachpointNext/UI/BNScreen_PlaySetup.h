#pragma once

#include "UI/BNActivatableWidget.h"
#include "BNScreen_PlaySetup.generated.h"

class UButton;
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
	 * The DETAILS panel's lines for this map, one entry per line, straight from ini.
	 *
	 * Free-form on purpose: the breakdown is the one place a map says something a schema
	 * cannot predict ("FIVE TIERS", "SPAWNS  16"), and inventing typed fields for it would
	 * mean a compile every time a map wants to say something new. Empty is legal — the panel
	 * falls back to its title alone.
	 */
	UPROPERTY(Config)
	TArray<FString> Details;
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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UButton> MapButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UButton> ModeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UButton> BotsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UButton> StartButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UButton> BackButton;

	// -- the values those rows print, and the breakdown panel's mirrors ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UTextBlock> MapValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UTextBlock> ModeValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UTextBlock> BotsValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UTextBlock> DescriptionText;

	/** The map plate. Optional so a WBP without one still compiles and still plays. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UImage> PreviewImage;

	/** The DETAILS panel's body — the selected map's own lines, rewritten as MAP cycles. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UTextBlock> BreakdownText;

	// -- data + state ------------------------------------------------------------------

	/** The menu's map roster, from ini. Empty = the designed miss: START logs and refuses,
	 *  the rows print the miss, nothing crashes. */
	UPROPERTY(Config)
	TArray<FBNFrontEndMapEntry> Maps;

	/** Lobby-size presets the BOTS row cycles. Total INCLUDING the human — the founder
	 *  counts the player ("eight bots in total counting the player"). */
	UPROPERTY(Config)
	TArray<int32> PlayerCountPresets = { 4, 8, 12, 16 };

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
};
