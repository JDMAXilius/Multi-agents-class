#pragma once

#include "FieldNotificationId.h"
#include "UI/BNActivatableWidget.h"
#include "BNScreen_Scoreboard.generated.h"

class UBNScoreRow;
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
UCLASS(Abstract, meta = (DisableNativeTick))
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
	void RefreshOutcome(const class UBNVM_Match* Match);
	void RefreshTeamScores(const class UBNVM_Match* Match);

	/** The WBP parents UBNScoreRow children under this — 8 covers FFA with headroom. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UPanelWidget> RowContainer;

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

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBNScoreRow>> Rows;

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Match> BoundViewModel;
	FDelegateHandle RosterHandle;

	bool bWarnedRowShortage = false;
};
