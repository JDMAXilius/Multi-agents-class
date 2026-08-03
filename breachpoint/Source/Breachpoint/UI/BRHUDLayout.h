#pragma once

#include "CommonUserWidget.h"
#include "UI/BRActivatableWidget.h"
#include "UI/BRUITypes.h"

#include "BRHUDLayout.generated.h"

class UCommonTextBlock;
class UImage;

/**
 * `UBRKillfeedEntryWidget` -- ONE killfeed row. Driven exclusively by `UBRKillfeed`
 * (`UI/HUD/BRKillfeed.h`), which owns the pool that holds it.
 *
 * BP66 CLOSED (HUD-CPP-AUDIT): this class used to carry ZERO BindWidget members and a
 * `BlueprintImplementableEvent`-only update path -- unimplementable under R18 (a BIE needs a
 * graph node; WBPs have empty graphs), so `SetEntry` stored a struct and rendered nothing.
 * The four members below are the exact contract `wbp_plan.py` pre-committed, so the planned
 * asset is correct with no re-authoring and `validate()` now enforces the match.
 *
 * NO COLOUR IS SET HERE, AND THAT IS LOAD-BEARING: `UBRKillfeed` tints the WHOLE ROW via
 * `SetColorAndOpacity`, which multiplies down the tree. White leaves are the correct input to
 * that multiply; a leaf colour here would double-tint every line.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRKillfeedEntryWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetEntry(const FBRKillfeedViewEntry& InEntry);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|Killfeed")
	TObjectPtr<UCommonTextBlock> KillerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|Killfeed")
	TObjectPtr<UCommonTextBlock> VictimNameText;

	/**
	 * Optional, and it RESERVES ITS SLOT: an empty spotter line renders as empty text, never
	 * collapses -- the feed must not reflow when the LLM answers late, and offline is the
	 * identical HUD minus the flavour (`wbp_plan.py`'s killfeed-entry notes).
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|Killfeed")
	TObjectPtr<UCommonTextBlock> SpotterLineText;

	/**
	 * Optional and COLLAPSED until weapon glyph art exists (none is imported yet). A brushless
	 * image renders as a blank white rectangle -- BP70 D2's defect -- so the slot stays
	 * collapsed rather than blank until `FBRKillfeedViewEntry` carries an icon path.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|Killfeed")
	TObjectPtr<UImage> WeaponIcon;

private:
	UPROPERTY(Transient)
	FBRKillfeedViewEntry Entry;
};

/**
 * `UBRHUDLayout` -- the in-game HUD frame. It hosts the surfaces, routes ViewModels into the
 * MVVM view, and declares the Game input config; it renders no feed, bar or number itself.
 *
 * THE ONE HUD ACTIVATABLE, DELIBERATELY. After the HUD-C re-base, every other HUD surface is a
 * `UCommonUserWidget`; this frame keeps `UBRActivatableWidget` because it is genuinely pushed
 * to `Layer.Game` and its input config is the fallback the whole input stack rests on when no
 * menu is up. "Activatable = screens only" is now grep-checkable with this class as the single
 * documented exception.
 *
 * THE KILLFEED IS NOT HERE, DELIBERATELY. This class used to carry a second killfeed that
 * projected the SAME `UBRVM_Match` ring as `UBRKillfeed` -- two projections of one array is one
 * feed drawn twice. HUD-CPP-AUDIT also removed this class's hit-marker and state-change
 * subscriptions: the four hit hooks duplicated the reticle's own `OnHitMarker` subscription
 * into `BlueprintImplementableEvent`s no WBP may implement (R18), and the two state hooks had
 * the same problem with no consumer even proposed. The surfaces render their own states; the
 * frame routes and stays out of the way.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRHUDLayout : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRHUDLayout();

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	virtual void BindViewModels() override;

private:
	void PushViewModelsIntoMVVMView();
};
