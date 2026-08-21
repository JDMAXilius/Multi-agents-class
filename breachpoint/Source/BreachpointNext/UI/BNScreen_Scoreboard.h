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
 * pool doctrine); the winner banner is the VM's phase text. No input mode change: hold-to-view
 * takes no clicks, and the post-match needs none either.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNScreen_Scoreboard : public UBNActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;

	void HandleRosterChanged();
	void HandleMatchFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);
	void Refresh();

	/** The WBP parents UBNScoreRow children under this — 8 covers FFA with headroom. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UPanelWidget> RowContainer;

	/** Winner / warmup line during post-match; empty and hidden while the match runs. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> BannerText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBNScoreRow>> Rows;

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Match> BoundViewModel;
	FDelegateHandle RosterHandle;

	bool bWarnedRowShortage = false;
};
