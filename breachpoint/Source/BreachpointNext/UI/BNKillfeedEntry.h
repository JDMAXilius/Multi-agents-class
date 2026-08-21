#pragma once

#include "CommonUserWidget.h"
#include "BNKillfeedEntry.generated.h"

class UTextBlock;

/** ONE killfeed row — its own header, deliberately (the old module fused two classes into one
 *  file and its whole asset pipeline grew a special case to cope). A row renders a line and a
 *  tint and decides nothing: whole-row tint via SetColorAndOpacity, which multiplies down the
 *  tree — which is why the WBP sets no leaf color at all. */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNKillfeedEntry : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Claim: fill and show. White = the local player is in this line; dim = history. */
	void SetEntry(const FText& Line, bool bInvolvesSelf);

	/** Release: collapse. The pool never destroys a row. */
	void ClearEntry();

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> LineText;
};
