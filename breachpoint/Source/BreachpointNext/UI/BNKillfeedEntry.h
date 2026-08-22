#pragma once

#include "CommonUserWidget.h"
#include "BNKillfeedEntry.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/** ONE killfeed row — its own header, deliberately (the old module fused two classes into one
 *  file and its whole asset pipeline grew a special case to cope). A row renders a line and a
 *  tint and decides nothing: whole-row tint via SetColorAndOpacity, which multiplies down the
 *  tree — which is why the WBP sets no leaf color at all. */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNKillfeedEntry : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Claim: fill and show. White = the local player is in this line; dim = history. The glyph
	 *  (R7.3) is the killing weapon's icon and is simply absent when the cause has no row or the
	 *  row has no art — the line never grows a text weapon, which would break its measured width. */
	void SetEntry(const FText& Line, bool bInvolvesSelf, const TSoftObjectPtr<UTexture2D>& WeaponIconAsset = TSoftObjectPtr<UTexture2D>());

	/** Release: collapse. The pool never destroys a row. */
	void ClearEntry();

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> LineText;

	/** The design's 22×8 weapon glyph. Optional: a feed without it still reads. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UImage> WeaponIcon;

	/** What is on the brush now — a claimed row re-renders on every feed change, and re-issuing
	 *  the same soft load would cancel and restart a stream that was nearly done. */
	TSoftObjectPtr<UTexture2D> AppliedIcon;
};
