#pragma once

#include "CommonUserWidget.h"
#include "BNScoreRow.generated.h"

class UImage;
class UTexture2D;
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
	/** The row's emblem (cycled by the board from its ini list; no per-player art exists yet). */
	void SetEmblem(const TSoftObjectPtr<UTexture2D>& InEmblem);
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

	/** Founder's capture (2 Sep): KDA = (K + A/3) − D; assists unknown → K − D, said so in the ticket. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> KdaText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UImage> Emblem;

	// RowFill / SelfBar / SelfCaret / CellTint0..3 are reached BY NAME in SetRow (type-safe Cast),
	// the lesson of the 2 Sep RefreshHeader crash.
};
