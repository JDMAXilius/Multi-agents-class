#pragma once

#include "UI/BNActivatableWidget.h"
#include "BNScreen_FrontEnd.generated.h"

class UBRButton;
class UBRNavBar;
class UBRRosterPanel;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/**
 * THE MAIN MENU — Menu layer, Menu input, the first thing a booted game shows.
 *
 * Figma truth: `01-MENU-MEASURED.md` §2 — FE_Play `21:32824`. One Menu Combo at (69,138)
 * 349×510 on the left third, the centre column is the 3D subject and the right is status.
 * The layout lives in the WBP; this class owns behaviour and strings, per the house split
 * (a WBP never types a string, C++ never places a pixel).
 *
 * TWO live rows, not four. The design's rail has four slots; this project today has
 * exactly two actions a button can honestly perform — PLAY and QUIT — and a button that
 * opens nothing is worse than a button that is absent (the pause screen's own ruling).
 * The WBP keeps the four-slot geometry; the two dead slots ship disabled with their
 * design names, which is what the reference game itself does with locked entries.
 *
 * Session shape (founder, 1 Sep): no online sessions — one computer, one player, bots.
 * PLAY therefore goes straight to the play-setup screen; there is no matchmaking search
 * and nothing here pretends there is.
 */
UCLASS(Abstract, Config = Game, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNScreen_FrontEnd : public UBNActivatableWidget
{
	GENERATED_BODY()

public:
	UBNScreen_FrontEnd();

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleQuitClicked();

	/**
	 * PLAY — pushes the play-setup screen on the same Menu stack, so back pops home.
	 *
	 * UBRButton, not UButton: this is the Menu Row component measured 1:1 against Figma
	 * `12:724`, and it is what carries the design's Idle→Hover INVERSION, the 28-high row and
	 * the token palette. An engine UButton renders the grey capsule the founder was seeing.
	 * The label is set from C++ through SetLabelText, so the WBP still types no strings.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UBRButton> PlayButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UBRButton> QuitButton;

	/**
	 * THE NAVIGATION BAR — `21:32864`, 33,45 666x30: four "Menu Slider Button" tabs,
	 * 138x26 at pitch 150 (x 39/189/339/489 inside the bar).
	 *
	 * Only PLAY has a screen behind it. The other three ship DISABLED with their design
	 * names rather than absent, for the same reason the rail's two dead rows do: a tab that
	 * opens nothing is worse than a tab that is visibly not ready, and deleting them would
	 * lose the measured 666-wide band the design balances the header on.
	 */
	/**
	 * The top nav strip — ONE measured component where four hand-placed canvas buttons and two
	 * "LB"/"RB" TextBlocks used to sit.
	 *
	 * `UBRNavBar` owns the geometry the canvas was re-deriving: BarWidth 666, BarHeight 30,
	 * FirstTabOffsetX 39, TabPitch 150, TabGap 12, tab 138 x 26 — node `21:32864`. It also owns
	 * what the canvas could not do at all: the tabs go in a `UCommonButtonGroupBase`, so
	 * selection exclusivity and gamepad routing are CommonUI's rather than hand-rolled, and the
	 * bumper prompts are `UBRButtonPrompt`s whose `CommonActionWidget` swaps LB/RB per platform
	 * instead of two hardcoded English strings.
	 *
	 * Tabs are DATA: `SetTabs` builds them at runtime from `TabWidgetClass`. The WBP ships an
	 * empty container and that is correct, not unfinished.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBRNavBar> NavBar;

	/**
	 * THE PROGRESSION PANEL — `21:32826`, 869,55 334x115 (title above a 334x94 border).
	 *
	 * There is NO progression system in this project. So the rank line and its bar are fed
	 * from ini, not invented in code: with nothing configured the line collapses and the bar
	 * reads zero, and the panel is honest chrome rather than a fake Sergeant Grade 1.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UTextBlock> RankText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UProgressBar> RankProgress;

	/** The career line, e.g. "SERGEANT  GRADE 1". EMPTY by default — we have no ranks yet. */
	UPROPERTY(Config)
	FString CareerRankLine;

	/** 0..1 fill for the rank bar. Meaningless until CareerRankLine is configured. */
	UPROPERTY(Config)
	float CareerRankFraction = 0.0f;

	/** The two design slots that have no action yet. Disabled, per the class comment. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBRButton> CustomsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBRButton> AcademyButton;

	/** The rail's hint strip (Description Frame, 349×37). Set from C++ on focus-worthy
	 *  moments; a WBP never types the copy. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UTextBlock> DescriptionText;

	/** The News card's art (349×222), painted BEHIND its title. Optional: with no texture
	 *  configured the Image collapses and the card is a tinted panel with a headline. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UImage> NewsImage;

	/** The plate the News card shows. SOFT (law 3) and from ini, so swapping the featured
	 *  arena is a config line rather than a compile — and it is OUR capture, never the
	 *  mock's 343-owned key-art (01-MENU-MEASURED §6). */
	UPROPERTY(Config)
	TSoftObjectPtr<UTexture2D> NewsImageTexture;

	/**
	 * The "IN MENUS" roster — ONE instance of the measured component, in place of the 23
	 * hand-built canvas widgets BN42 originally put here (a Border, a header, six plates, six
	 * avatars, six names, two notches and a privacy label).
	 *
	 * `UBRRosterPanel`'s own constants ARE the Figma numbers off `12:39611` — PanelOriginX 862,
	 * PanelOriginY 397, 349 x 273, BackgroundInset 3, ContentInset 16, HeaderY 16, FirstRowY 52,
	 * MaxVisibleRows 6, and `UBRRosterRow`'s RowHeight 30 / RowPitch 35. Re-deriving any of that
	 * on a canvas was the mistake; the component is the measurement.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBRRosterPanel> RosterPanel;

	/**
	 * Who the roster lists. Config, not literals in the WBP — the six names used to be typed
	 * into six CommonTextBlocks inside the asset, which is a string in a binary the C++ cannot
	 * see. Entry 0 is the local player and the party leader.
	 *
	 * There is no party system yet (`GAP: UBRVM_Lobby`, SCREEN-MANIFEST §Screens). When one
	 * lands it replaces this array; until then the honest fill is data a human wrote down, not
	 * a roster C++ invented.
	 */
	UPROPERTY(Config)
	TArray<FString> RosterNames;

	/** Shared roster emblem. SOFT (law 3), and it is a FRAME, not a portrait — we have no avatars. */
	UPROPERTY(Config)
	TSoftObjectPtr<UTexture2D> RosterEmblem;

	/**
	 * The rank diamond, rendered from Figma `12:39621`'s own path data by
	 * `Tools/bn/bn46_roster_icons.py`. Shared across the roster because this project has no
	 * per-player rank: one placeholder crest is honest, six different ones would be fiction.
	 * SOFT, and null simply collapses the rank cell — `SetShown(RankFrame, !RankInsignia.IsNull())`.
	 */
	UPROPERTY(Config)
	TSoftObjectPtr<UTexture2D> RosterRankInsignia;
};
