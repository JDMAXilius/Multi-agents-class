#pragma once

#include "FieldNotificationId.h"
#include "UI/BRActivatableWidget.h"
#include "UI/BRUITypes.h"

#include "BRScreen_Scoreboard.generated.h"

class UCommonTextBlock;
class UPanelWidget;
class USizeBox;
class UWidget;

/**
 * A measured rectangle from the Figma read. Plain aggregate, deliberately NOT a USTRUCT: nothing
 * reflected consumes it, it exists so a review can diff one line against one measurement instead
 * of chasing four loose floats per box.
 */
struct FBRScoreboardRect
{
	float X = 0.0f;
	float Y = 0.0f;
	float W = 0.0f;
	float H = 0.0f;
};

/**
 * `UBRScreen_Scoreboard` -- HUD Scoreboard (TAB), Figma frame `43:2`, measured 1:1 into
 * `Tools/gen_ui/figma_screen_layout.json` (`scoreboard` block, read 2026-08-02). WBP:
 * `WBP_Screen_Scoreboard`.
 *
 * THE MATCH IS LIVE WHILE THIS IS UP. The frame is named "Scene (blurred -- does NOT pause a live
 * match)". Two consequences that are not style preferences:
 *   1. It pushes to `Layer.Game` (`FBRUITags::Layer_Game`), never `Layer.Menu` / `Layer.Modal`.
 *   2. `GetDesiredInputConfig()` returns `ECommonInputMode::Game` with the viewport still
 *      captured -- see the override. A scoreboard that takes exclusive Menu input drops the
 *      player's look and fire input in a live firefight.
 * It is not a back handler: TAB-up pops it, and stealing the back action from whatever is beneath
 * would make B/Escape close the scoreboard instead of opening the pause menu.
 *
 * IT KEEPS UPDATING WHILE SHOWN, and it does that with zero Tick: `UBRVM_Match` already owns the
 * clock timer and pushes FieldNotify changes, so the WBP's MVVM bindings repaint themselves.
 *
 * DATA (read the CONTRACT GAP below before authoring against this class): the only source is
 * `UBRVM_Match`. It carries the match frame -- clock, phase, `Team0Score` / `Team1Score`,
 * `LocalTeamId`, killfeed -- and NO per-player collection at all. So the two team blocks and the
 * header are live; the eight-row roster table renders the honest unknown state and nothing else.
 * This class invents no player struct to paper over that.
 *
 * -----------------------------------------------------------------------------------------------
 * CONTRACT GAP (netcode-builder / the ViewModel owner) -- `UBRVM_Match` has no roster.
 * The screen needs ONE FieldNotify array of already-formatted rows, pushed the same way the
 * killfeed is, plus its own `EBRUIDataState` so "not replicated yet" is distinguishable from
 * "empty". Shape asked for, per player:
 *     FText   DisplayName;      // gamertag, pre-formatted by the producer
 *     FText   ServiceTag;       // right-aligned after the tag; empty renders the dash
 *     int32   Score, Kills, Assists, Deaths;   // INDEX_NONE means unknown, not zero
 *     uint8   TeamId;           // matches UBRVM_Match::GetLocalTeamId()
 *     bool    bIsLocalPlayer;   // drives the highlight row + its overhanging accent
 *     bool    bIsAlive;         // the 7x7 status dot at row-local x=8
 * plus `EBRUIDataState RosterState` and an `OnRosterChanged` multicast, mirroring
 * `OnKillfeedChanged`. SORTING IS THIS SCREEN'S JOB and is deliberately not requested: the
 * ViewModel owns the values, presentation owns the order. Until that lands there is nothing to
 * sort -- `RosterContainer` stays collapsed and `RosterUnavailableText` shows the dash.
 * -----------------------------------------------------------------------------------------------
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRScreen_Scoreboard : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRScreen_Scoreboard();

	// ===============================================================================================
	// MEASURED GEOMETRY -- Figma `43:2`, canvas coordinates unless a name says row-local.
	// Restated in source so a review checks the WBP against a number here instead of against a
	// document. Nothing below is a guess; anything unmeasured is absent rather than plausible.
	// ===============================================================================================

	/** Content box: the header rule spans x=100..1159, and the table's right edge is that same 1159. */
	static constexpr float ContentLeftX = 100.0f;
	static constexpr float ContentRightX = 1159.0f;

	// -- Header band -------------------------------------------------------------------------------

	/** Mode / map caption row and the 2px rule under it. */
	static constexpr FBRScoreboardRect HeaderModeText{100.0f, 34.0f, 182.0f, 33.0f};
	static constexpr float HeaderSeparatorX = 358.0f;
	static constexpr FBRScoreboardRect HeaderMapText{385.0f, 34.0f, 129.0f, 33.0f};
	static constexpr FBRScoreboardRect HeaderRule{100.0f, 67.0f, 1059.0f, 2.0f};
	static constexpr FBRScoreboardRect HeaderWinCondition{100.0f, 73.0f, 119.0f, 17.0f};
	static constexpr FBRScoreboardRect HeaderClock{282.0f, 73.0f, 35.0f, 15.0f};

	// -- Team blocks (left of the table) ------------------------------------------------------------
	//
	// Team slot 0 is the TOP block, slot 1 the bottom; they map to `UBRVM_Match::GetTeam0Score()` /
	// `GetTeam1Score()` in that order. The block is NOT re-ordered by `GetLocalTeamId()` -- the frame
	// fixes the order and a scoreboard that rearranges itself per client is unreadable in a callout.

	static constexpr float TeamBlockPitchY = 67.0f;
	static constexpr float TeamBlockTopY = 268.0f;
	static constexpr float TeamBlockBottomY = 335.0f;
	static constexpr FBRScoreboardRect TeamBlockAccent{96.0f, 0.0f, 4.0f, 44.0f};
	static constexpr FBRScoreboardRect TeamBlockEmblem{100.0f, 0.0f, 44.0f, 44.0f};
	static constexpr float TeamBlockEmblemArtInset = 6.0f;
	static constexpr FBRScoreboardRect TeamBlockNameplate{145.0f, 0.0f, 142.0f, 44.0f};
	static constexpr FBRScoreboardRect TeamBlockScore{287.0f, 0.0f, 67.0f, 44.0f};

	// -- Table -------------------------------------------------------------------------------------

	static constexpr float TableX = 465.0f;
	static constexpr float TableWidth = 694.0f;
	static constexpr float RowHeight = 22.0f;
	static constexpr float RowPitch = 22.0f;

	static constexpr float ColumnHeaderY = 157.0f;
	static constexpr float HeaderRuleY = 173.0f;

	/** The table body origin. Every row-local / overlay offset below is measured from HERE. */
	static constexpr float TableBodyTopY = 191.0f;
	static constexpr float TableBodyBottomRuleY = 472.0f;

	/**
	 * The alternating column tints: 83 x 282 at canvas x=825 and x=992, running the FULL table
	 * height across both team fills AND the divider. They are TABLE FURNITURE, not row state --
	 * authored once behind the rows, never per row, and never recoloured by anything a player does.
	 * Note 191 + 282 = 473, i.e. the tint runs 1px under the bottom rule at 472. Measured, kept.
	 */
	static constexpr FBRScoreboardRect ColumnTintA{825.0f, 191.0f, 83.0f, 282.0f};
	static constexpr FBRScoreboardRect ColumnTintB{992.0f, 191.0f, 83.0f, 282.0f};

	/**
	 * ONE contiguous fill per team, 694 x 88 (= 4 rows x 22). NOT four per-row backgrounds: at 22px
	 * pitch a per-row plate shows a seam on every row boundary.
	 */
	static constexpr FBRScoreboardRect TeamFillTop{465.0f, 224.0f, 694.0f, 88.0f};
	static constexpr FBRScoreboardRect TeamFillBottom{465.0f, 378.0f, 694.0f, 88.0f};
	static constexpr float TeamDividerY = 367.0f;

	// -- Columns -----------------------------------------------------------------------------------

	/** Row-local. Status dot 7x7 at (8, 7.5); rank at 31; gamertag at 51; service tag right-aligned after it. */
	static constexpr FBRScoreboardRect RowStatusDot{8.0f, 7.5f, 7.0f, 7.0f};
	static constexpr float RowRankX = 31.0f;
	static constexpr float RowGamertagX = 51.0f;

	/**
	 * THE 5px DISAGREEMENT, RULED -- read this before "fixing" a column.
	 *
	 * The measurement records two different sets of stops for the same four columns:
	 *   cells   (row-local)  356.5 / 440 / 523 / 606.5  -> canvas 821.5 / 905 / 988 / 1071.5
	 *   headers (canvas)     816.5 / 900 / 983 / 1066.5
	 * a uniform -5 on the header group.
	 *
	 * RULING: THE CELLS WIN. The headers move +5. Evidence is the third measurement, which nobody
	 * had to reconcile by hand -- the column tints. `ColumnTintA` spans canvas 825..908, centre
	 * 866.5; the SCORE cell spans 821.5..911.5, centre 866.5 -- EXACT. `ColumnTintB` centre is
	 * 1033.5 against the ASSISTS cell centre 1033.0. The header captions miss both by 5. Two of the
	 * three measured systems agree on the cells, and the offset is uniform across all four headers,
	 * which is the signature of a Figma group that was never nudged back.
	 *
	 * So: these constants are the ONE set of stops, and the header captions are laid out from them
	 * (`ColumnHeaderY` for the baseline, `TableX + <stop>` for x). Header and cell driven from one
	 * array is the only thing that stops them drifting by a pixel forever.
	 */
	static constexpr float ColumnWidth = 90.0f;
	static constexpr float ColumnScoreX = 356.5f;
	static constexpr float ColumnKillsX = 440.0f;
	static constexpr float ColumnAssistsX = 523.0f;
	static constexpr float ColumnDeathsX = 606.5f;

	// -- Local-player highlight --------------------------------------------------------------------

	/**
	 * THE ACCENT OVERHANGS THE TABLE AND THAT IS CORRECT. The highlight row starts at the table's
	 * left edge (x=465); its 4x22 accent bar starts at x=460, i.e. 5px OUTSIDE the table. An HBox
	 * cannot express a child that begins left of its parent's content box, so the highlight is an
	 * OVERLAY child with a negative horizontal offset -- applied here in C++, from
	 * `LocalAccentOverhangX`, so that no details panel can quietly "correct" it to 465.
	 */
	static constexpr float LocalAccentOverhangX = -5.0f;
	static constexpr float LocalAccentWidth = 4.0f;

	/** Row 0 of a team block, measured from `TableBodyTopY`. Top: 224-191. Bottom: 378-191. */
	static constexpr float TeamFillTopOffsetY = TeamFillTop.Y - TableBodyTopY;
	static constexpr float TeamFillBottomOffsetY = TeamFillBottom.Y - TableBodyTopY;

	/** 4 rows per team block -- the fill height is exactly 4 x `RowPitch`. */
	static constexpr int32 RowsPerTeam = 4;

	// ===============================================================================================

	/**
	 * Place the local player's highlight row + its overhanging accent. `TeamSlot` is 0 (top block)
	 * or 1 (bottom); `RowInTeam` is 0..3. Anything out of range clears the highlight rather than
	 * drawing it in a plausible wrong place.
	 *
	 * Nothing calls this yet -- see the CONTRACT GAP: no roster source means no local row index, so
	 * the honest state is "hidden", which is also the state this class starts in.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetLocalPlayerRow(int32 TeamSlot, int32 RowInTeam);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearLocalPlayerRow();

	/**
	 * THE MATCH IS LIVE. Game input, viewport still captured, cursor hidden -- the player can still
	 * look, move and shoot with the scoreboard up. Declared in C++ and not left to `InputMode` in a
	 * details panel for the same reason `UBRModal_Options` declares Menu there: an input mode that a
	 * WBP can flip is an input mode that will be flipped.
	 *
	 * If this screen ever grows an interactive element (report / mute a player), this becomes
	 * `ECommonInputMode::All` -- NOT `Menu`, which would still suspend gameplay input.
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnInitialized() override;

	//~ Begin UBRActivatableWidget interface
	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;
	//~ End UBRActivatableWidget interface

	// -----------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_Screen_Scoreboard`. `Tools/gen_ui/wbp_plan.py` parses these names
	// out of this header; the WBP's widget names must match exactly.
	//
	// `RosterContainer`, `LocalHighlightRow` and `LocalAccentBar` are Overlay children anchored
	// TOP-LEFT at the table body origin (canvas 465, 191) with zero padding. C++ supplies every
	// offset from there; the WBP supplies no x/y of its own for these three or the overhang is lost.
	// -----------------------------------------------------------------------------------------------

	/** C++ drives 694 x 282 so the table cannot be stretched from a details panel. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> TableSizeBox;

	/** Holds the eight roster rows. Collapsed while there is no roster source -- see the gap. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> RosterContainer;

	/**
	 * The honest empty state. Shows `BRUI::UnknownValueText()`, never "No players" and never a
	 * plausible-looking zero row: for the first frames after join or travel the truth is "not known
	 * yet", and today it is also the truth for the whole match.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> RosterUnavailableText;

	/** 694 x 22. Positioned by `SetLocalPlayerRow`; hidden until there is a local row to point at. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> LocalHighlightRow;

	/** 4 x 22, pushed `LocalAccentOverhangX` OUTSIDE the table. See the constant. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> LocalAccentBar;

private:
	void PushViewModelsIntoMVVMView();

	void HandleViewModelFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	/** One place decides what the roster region says. The roster source hooks in HERE when it lands. */
	void ApplyRosterState(EBRUIDataState InMatchState);

	void ApplyLocalHighlight();

	FDelegateHandle MatchStateFieldHandle;

	/** INDEX_NONE = no local row known. The starting state, and the state after every deactivate. */
	int32 LocalHighlightTeamSlot = INDEX_NONE;
	int32 LocalHighlightRowInTeam = INDEX_NONE;
};

// Outside the class body on purpose: UHT parses the body, and these belong to nobody's reflection.
static_assert(
	UBRScreen_Scoreboard::TableX + UBRScreen_Scoreboard::LocalAccentOverhangX == 460.0f,
	"The local-player accent is measured at canvas x=460 -- 5px OUTSIDE the table. Figma 43:2.");

static_assert(
	UBRScreen_Scoreboard::TeamFillTop.H == UBRScreen_Scoreboard::RowsPerTeam * UBRScreen_Scoreboard::RowPitch,
	"A team fill is ONE contiguous rect of exactly 4 rows; per-row backgrounds would show seams.");
