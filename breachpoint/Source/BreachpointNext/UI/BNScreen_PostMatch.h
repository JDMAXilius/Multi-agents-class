#pragma once

#include "FieldNotificationId.h"
#include "UI/BNActivatableWidget.h"
#include "BNScreen_PostMatch.generated.h"

class UBNPromptButton;
class UBNVM_Match;
class UBRButton;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/**
 * `UBNScreen_PostMatch` — the PLAYER RECAP page, the match's win/lose screen.
 *
 * Founder (2 Sep 2026): "do the win/lose screen widget, one to one with this [the shipped recap
 * capture] but add the win lose somewhere". The capture is a 1920x1080 frame (x1.5 of 1280x720);
 * every number in `Tools/bn/bn52_recap_1to1.py` is measured off it. The VICTORY / DEFEAT / DRAW
 * word sits top-right above the three header lines, in the outcome colour the scoreboard's band
 * already uses (Shield for a win, Threat for a loss).
 *
 * HONEST NUMBERS ONLY. The capture shows CAPTURES / RETURNS / STEALS / medals / a rank ladder.
 * This game tracks kills, deaths and score; assists and medals are not tracked; there is no
 * progression. So the six columns are KILLS · DEATHS · ASSISTS · KDA · SCORE · MEDALS with
 * dashes where the number does not exist, the medal row is the measured structure with dashes,
 * and the rank block prints the ini's career line (the same one the front end shows). A fake
 * "x2" would be a lie dressed as a feature; the dashes say what is missing.
 *
 * PAGES. The tabs are the capture's. SCOREBOARD pushes `UBNScreen_Scoreboard` over this page;
 * its PLAYER RECAP tab pops it again. TEAM LINEUP has no page yet and is placed dim.
 * The director pushes this screen on WaitingPostMatch when the manager has a class for it,
 * and falls back to pinning the scoreboard (the pre-BN45 behaviour) when it has not.
 */
UCLASS(Abstract, Config = Game, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNScreen_PostMatch : public UBNActivatableWidget
{
	GENERATED_BODY()

public:
	UBNScreen_PostMatch();

protected:
	virtual void NativeOnInitialized() override;
	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;

	void HandleRosterChanged();
	void HandleMatchFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);
	void Refresh();

	UFUNCTION()
	void HandleClosePrompt();

	UFUNCTION()
	void HandleBoardTab();

	/** Tints for the colour-free chrome the layout script places; by name, type-safe. */
	void TintChrome(const TCHAR* Name, const FLinearColor& Colour);

	// -- tabs
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBRButton> TabRecap;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBRButton> TabLineup;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBRButton> TabBoard;

	// -- outcome + header
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> OutcomeText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> HeaderLine1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> HeaderLine2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> HeaderLine3;

	// -- score + rank
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> ScoreValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> RankLineText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UProgressBar> RankProgress;

	// -- medal text
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> MedalNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> MedalDescText;

	// -- nameplate
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> NameplateRank;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> NameplateName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> NameplateSub;

	// -- legend
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBNPromptButton> ClosePrompt;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBNPromptButton> MatchmakingPrompt;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UBNPromptButton> ProfilePrompt;

	/** The career line the front end shows, until a progression system exists (ini). */
	UPROPERTY(Config)
	FString RankLine;

	UPROPERTY(Config)
	float RankFraction = 0.0f;

private:
	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Match> BoundViewModel;
	FDelegateHandle RosterHandle;
};
