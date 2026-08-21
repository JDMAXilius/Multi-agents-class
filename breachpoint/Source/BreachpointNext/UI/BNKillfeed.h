#pragma once

#include "CommonUserWidget.h"
#include "BNKillfeed.generated.h"

class UBNKillfeedEntry;
class UBNVM_Match;
class UPanelWidget;

/**
 * Bottom-left, the pool doctrine: the rows are FIXED CHILDREN of the WBP's container — placed
 * once by the asset, collected once at initialize, claimed and released forever after. Zero
 * CreateWidget on the per-kill path; a firefight allocates nothing. The VM's view ring is
 * authoritative and this widget is a pure projection over it: one delegate, full re-claim —
 * with ≤5 rows a diff would cost more than it saves.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNKillfeed : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void HandleKillfeedChanged();
	void Refresh();

	/** The WBP parents UBNKillfeedEntry children under this; anything else in it is ignored. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UPanelWidget> EntryContainer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBNKillfeedEntry>> Rows;

	TWeakObjectPtr<UBNVM_Match> BoundViewModel;
	FDelegateHandle KillfeedHandle;

	/** One-shot: pool exhaustion drops the newest-over-capacity and SAYS so once — a silent cap
	 *  reads as "covered everything" when it didn't. */
	bool bWarnedRowShortage = false;
};
