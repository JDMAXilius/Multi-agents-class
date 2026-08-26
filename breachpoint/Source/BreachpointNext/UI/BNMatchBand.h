#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "Styling/SlateColor.h"
#include "BNMatchBand.generated.h"

class UBNVM_Match;
class UProgressBar;
class UTextBlock;

/**
 * Bottom-centre: my score · the clock · the leader and the limit — or, in teams mode, my
 * side's score and theirs through the SAME readouts, relative and relation-tinted (BN16).
 * UCommonUserWidget, NOT the
 * activatable base — HUD surfaces are never pushed to a layer, so activation scope is construct
 * scope, and the activatable base costs bSupportsActivationFocus (the old module's paid lesson).
 * Push-only: every value arrives by FieldNotify; the clock digit flips because the VM's
 * phase-locked timer flipped it, never because this widget asked.
 *
 * BindWidget types are plain UTextBlock — a deliberate, recorded deviation from the compiled
 * reference's UCommonTextBlock: the Common variant demands a style asset per text, and R7 ships
 * zero style assets. The binding pattern is otherwise the transcription.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNMatchBand : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void BindMatchField(UBNVM_Match* Match, UE::FieldNotification::FFieldId FieldId);
	void HandleMatchFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);
	void Refresh();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> MyKillsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> TopKillsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> ScoreLimitText;

	/** R7.7, gap 1 — the design's two score bars, as fractions of the limit. Optional: the band
	 *  reads perfectly well as numbers alone, which is what it has been doing. The mode PIPS from
	 *  the same design node are still absent and still honest — nothing in this project models a
	 *  game mode, so four static blocks would be decoration pretending to be state. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UProgressBar> SelfScoreBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UProgressBar> TopScoreBar;

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Match> BoundViewModel;

	/** TEAMS (BN16): whether the relative-team tints are currently on the two readouts. The
	 *  tint is applied ONLY on a mode flip — the FFA refresh path stays today's instruction
	 *  stream, which is the teams-OFF proof this widget owes. */
	bool bTeamTintApplied = false;

	/** TopKillsText's tint as the WBP left it, captured at initialize — the band never set that
	 *  leaf's color before teams did, so restoring anything ELSE on a flip back to FFA would
	 *  invent a color today's band never showed. */
	FSlateColor DefaultTopKillsTint;
};
