#pragma once

#include "FieldNotificationId.h"
#include "UI/BNActivatableWidget.h"
#include "BNScreen_Scoreboard.generated.h"

class UBNPromptButton;
class UBNScoreRow;
class UBRButton;
class UTexture2D;
class UBNVM_Match;
class UPanelWidget;
class UTextBlock;

/**
 * The scoreboard — Game layer, held by Tab or pinned by the post-match, both the DIRECTOR's
 * decisions. Rows are fixed WBP children claimed over the VM's sorted roster (the killfeed's
 * pool doctrine); in teams mode the claim ORDER partitions the same pooled rows into two
 * blocks, my side first (see Refresh); the winner banner is the VM's phase text. No input mode
 * change: hold-to-view takes no clicks, and the post-match needs none either.
 */
UCLASS(Abstract, Config = Game, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNScreen_Scoreboard : public UBNActivatableWidget
{
	GENERATED_BODY()

public:
	UBNScreen_Scoreboard();

protected:
	virtual void NativeOnInitialized() override;
	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;

	void HandleRosterChanged();
	void HandleMatchFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);
	void Refresh();

	/** Grows the row pool to `Needed` (capped by MaxScoreRows). Returns rows available. */
	int32 EnsureRowCapacity(int32 Needed);

	/** Sizes the list box to the rows actually shown, and follows it with the bottom rule. */
	void LayoutRowBlock(int32 VisibleRows);
	void RefreshOutcome(const class UBNVM_Match* Match);
	void RefreshTeamScores(const class UBNVM_Match* Match);

	/** `43:8/10/12/14`: mode · map, and the result line "MATCH WON · SCORE: a-b · DURATION: m:ss". */
	void RefreshHeader(const class UBNVM_Match* Match);

	/** Close = leave the match (the pause screen's own route). The other prompts are the
	 *  capture's legend, verbs only, until their screens exist. */
	UFUNCTION()
	void HandleClosePrompt();

	/**
	 * The WBP parents UBNScoreRow children under this. The placed children are the POOL'S
	 * SEED, not its limit: the WBP ships 8, and anything past that is cloned at runtime from
	 * the seed's own class (see EnsureRowCapacity), so 6v6 and 8v8 list every player without
	 * a WBP edit. Before that, a 16-player lobby silently dropped its last 8 rows.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UPanelWidget> RowContainer;

	/** The hairline under the list. Optional; when bound it follows a grown list down. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UWidget> TableBottomRule;

	/** Winner / warmup line during post-match; empty and hidden while the match runs. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> BannerText;

	/**
	 * The outcome band (Figma `35:2` "Outcome Bar", 1280x96 with a 6px accent stripe).
	 *
	 * Separate from BannerText on purpose: the banner says WHO won ("Ossian WINS"), which is
	 * the same sentence for everyone in the match. This says what happened to YOU, and two
	 * players reading the same scoreboard must see different words. Both are Optional — a
	 * board without either still lists the scores.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> OutcomeText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<class UImage> OutcomeAccent;

	/**
	 * TEAMS (BN16): the board's team-ledger header — my side's points and theirs, MY SIDE FIRST
	 * (the relative-presentation law: no team id, no absolute team color ever reaches this
	 * screen; "my" is blue and "theirs" is red on both clients). Both Optional in the
	 * BannerText/OutcomeText pattern — a board that binds neither still lists the rows — and
	 * both Collapsed outside teams mode, so the FFA board takes no layout for a header that
	 * has not happened.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> MyTeamScoreText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> EnemyTeamScoreText;

	// -- `43:2` header and team cards; Optional so the pre-BN44 WBP still binds.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> ModeText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> MapText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> ResultLineText;

	/** Relative by law (`EBNUITeamRelation`): YOUR TEAM / ENEMY TEAM until a ruling on literals. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> MyTeamNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> EnemyTeamNameText;

	/** The capture's tab bar. Only SCOREBOARD is a page today; the other two are placed, dim. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBRButton> TabRecap;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBRButton> TabLineup;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBRButton> TabBoard;

	/** Team card rank digits: 1 for the leading team, 2 for the other. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> MyTeamRankText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> EnemyTeamRankText;

	/** Bottom legend, as buttons. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBNPromptButton> ClosePrompt;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBNPromptButton> ViewPrompt;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBNPromptButton> MatchmakingPrompt;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBNPromptButton> ReportPrompt;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBNPromptButton> CyclePrompt;

	/** Row emblems, cycled by row index — no per-player art exists yet (SOFT, law 3). */
	UPROPERTY(Config)
	TArray<TSoftObjectPtr<UTexture2D>> RowEmblems;

	/** The capture: a gap with a 1px divider between the two team blocks. */
	UPROPERTY(Config)
	float TeamGap = 9.0f;

	// Chrome images (HeaderTick, ColumnTintA/B, TeamDivider, My/EnemyTeamAccent) are reached BY
	// NAME in RefreshHeader — see the note there; no typed binds for them.

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBNScoreRow>> Rows;

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Match> BoundViewModel;
	FDelegateHandle RosterHandle;

	bool bWarnedRowShortage = false;

	/**
	 * Hard ceiling on cloned rows. 16 is the largest lobby the front end can start
	 * (PlayerCountPresets tops out at 16 = 8v8); the cap exists so a runaway roster cannot
	 * spawn widgets without bound, not to express a design limit.
	 */
	UPROPERTY(Config)
	int32 MaxScoreRows = 16;

	/**
	 * One row's height, matching WBP_BNScoreRow's RootSizeBox HeightOverride (22). C++ cannot
	 * ask the row for this before layout has run, so it is stated here and in the asset; if
	 * the asset's SizeBox changes, this changes with it.
	 */
	UPROPERTY(Config)
	float ScoreRowHeight = 22.0f;

	/** The list box and rule as the WBP AUTHORED them — restored whenever the roster fits. */
	float AuthoredListHeight = 0.0f;
	float AuthoredRuleY = 0.0f;
	bool bCapturedAuthoredLayout = false;
};
