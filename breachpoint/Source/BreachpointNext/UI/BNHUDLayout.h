#pragma once

#include "FieldNotificationId.h"
#include "UI/BNActivatableWidget.h"
#include "BNHUDLayout.generated.h"

class UBNVM_Combat;
class UBNVM_Match;
class UImage;
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

	/**
	 * The centre reticle. It lives here for the SAME reason the banner does — its anchor
	 * (dead centre) belongs to no surface — and it is the one thing on this layout fed by the
	 * COMBAT view model rather than the match one, because a reticle changes with the weapon
	 * in your hands, not with the scoreline.
	 *
	 * DefaultReticle is the fallback for a row whose Reticle column is unset, and for the
	 * unarmed hand. An FPS whose aiming mark disappears when you pick up an unconfigured gun
	 * is a bug; honest-unknown does not apply to a crosshair.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UImage> ReticleDot;

	UPROPERTY(EditDefaultsOnly, Category = "BN|HUD")
	TSoftObjectPtr<UTexture2D> DefaultReticle;

	void BindCombatField(UBNVM_Combat* Combat, UE::FieldNotification::FFieldId FieldId);
	void HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);
	void RefreshReticle();

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Match> BoundViewModel;

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundCombatFields;
	TWeakObjectPtr<UBNVM_Combat> BoundCombatViewModel;

	/** 5c: announce a missing reticle ONCE, on the edge. A per-swap warning would spam. */
	bool bWarnedNoReticle = false;
};
