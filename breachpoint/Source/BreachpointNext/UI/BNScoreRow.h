#pragma once

#include "CommonUserWidget.h"
#include "BNScoreRow.generated.h"

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
};
