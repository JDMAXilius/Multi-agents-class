#pragma once

#include "CommonUserWidget.h"
#include "BNKillfeedEntry.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/** ONE killfeed row — its own header, deliberately (the old module fused two classes into one
 *  file and its whole asset pipeline grew a special case to cope). A row renders a line and a
 *  tint and decides nothing: whole-row tint via SetColorAndOpacity, which multiplies down the
 *  tree — which is why the WBP sets no leaf color at all. Teams (BN16) tint the killer/victim
 *  PARTS by relation, still code-driven and still no WBP color: the row goes neutral and the
 *  leaves carry the hue (multiplying Ally through InkDim would be neither), with the
 *  bInvolvesSelf white row keeping full priority — see SetEntry. */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNKillfeedEntry : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Claim: fill and show. White = the local player is in this line; dim = history.
	 *
	 *  ONE ARGUMENT, the view entry (R7.6): this row grew from a line to a line-plus-glyph to a
	 *  three-part layout, and each step added a parameter to every caller. The view struct is
	 *  already the thing the feed hands out.
	 *
	 *  TWO LAYOUTS, decided HERE and not by the caller: with `KillerText`/`VictimText` bound AND
	 *  the entry carrying both names, the row draws [Killer][glyph][Victim] at the design's
	 *  measured x. Otherwise it draws the composed `Line` — which is also the only correct render
	 *  for the wordings that have no killer at all. */
	void SetEntry(const struct FBNKillfeedViewEntry& Entry);

	/** Release: collapse. The pool never destroys a row. */
	void ClearEntry();

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> LineText;

	/** R7.6 — the parts layout. Optional AND paired: one without the other is not a layout, so
	 *  the row falls back rather than drawing half a line. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> KillerText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> VictimText;

	/** The design's 22×8 weapon glyph. Optional: a feed without it still reads. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UImage> WeaponIcon;

	/** What is on the brush now — a claimed row re-renders on every feed change, and re-issuing
	 *  the same soft load would cancel and restart a stream that was nearly done. */
	TSoftObjectPtr<UTexture2D> AppliedIcon;
};
