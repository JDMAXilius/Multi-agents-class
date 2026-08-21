#pragma once

#include "FieldNotificationId.h"
#include "UI/BNActivatableWidget.h"
#include "BNHUDLayout.generated.h"

class UBNVM_Match;
class UTextBlock;

/**
 * The in-match HUD: the one Game-layer activatable, alive for the whole match. It owns almost
 * nothing itself — the surfaces inside it (vitals, ammo, band, killfeed) are their own classes
 * placed by the WBP and self-binding — except the PHASE BANNER, whose anchor (upper center)
 * belongs to no surface.
 *
 * The six HUD rules, enforced here where they can be:
 *  - HitTestInvisible root-to-leaf (set ONCE on this root; it propagates) — a Visible HUD
 *    swallows every click and presents as "the game stopped responding".
 *  - No focus, ever: an activatable that takes focus steals the controller from the game.
 *  - Input config Game (the base's), no mouse capture change.
 *  - No SafeZone wrapper on the canvas; no Tick anywhere; the WBP holds layout only.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNHUDLayout : public UBNActivatableWidget
{
	GENERATED_BODY()

public:
	UBNHUDLayout();

protected:
	virtual void NativeOnInitialized() override;
	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;

	void HandleMatchFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);
	void Refresh();

	/** WARMUP / winner banner. Optional: a HUD without it still runs, minus the words. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> BannerText;

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Match> BoundViewModel;
};
