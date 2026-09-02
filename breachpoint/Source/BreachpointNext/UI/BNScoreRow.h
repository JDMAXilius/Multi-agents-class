#pragma once

#include "CommonUserWidget.h"
#include "BNScoreRow.generated.h"

class UImage;
class UTextBlock;
struct FBNScoreRowView;

/** One scoreboard row: name · kills · deaths. The killfeed entry's twin — claimed and released
 *  by the scoreboard over rows the WBP placed, tinted whole (white = you, cyan name = the
 *  winner, dim = everyone else; teams add ally blue / enemy red BY RELATION, never by team id),
 *  deciding nothing. */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNScoreRow : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetRow(const FBNScoreRowView& View);
	void ClearRow();

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> KillsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> DeathsText;

	// -- `43:2` leaves the measured row also carries; all Optional so the old WBP keeps working.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> AssistsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> TagText;

	/** `43:39` — the local player's row is a FILL, not a tint. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UImage> HighlightFill;

	/** `43:57` — 4x22 accent standing 5px proud of the row's left edge. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UImage> HighlightAccent;
};
