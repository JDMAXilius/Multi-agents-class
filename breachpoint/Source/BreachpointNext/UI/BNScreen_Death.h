#pragma once

#include "FieldNotificationId.h"
#include "UI/BNActivatableWidget.h"
#include "BNScreen_Death.generated.h"

class UBNVM_Combat;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/**
 * The death overlay — GameMenu layer, pushed and popped by the DIRECTOR on the State.Dead tag;
 * this widget decides nothing about its own life. Informational only: input stays the game's
 * (Inherit — the dead can look around), respawn is automatic, and the two lines it renders are
 * VM state: who ("Eliminated by X", threat red — the one message that earns that channel) and
 * when ("Respawning in N", amber — a running clock).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNScreen_Death : public UBNActivatableWidget
{
	GENERATED_BODY()

public:
	UBNScreen_Death();

protected:
	virtual void NativeOnInitialized() override;
	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;

	void HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);
	void Refresh();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> KilledByText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> RespawnText;

	/** R7.3 — the design's second line: WHAT they used. Hidden when the death has no named
	 *  cause, so the screen never shows an empty row where a weapon should be. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> WeaponText;

	/** R7.7, gap 4 — the design's three missing pieces, all optional so a HUD without them still
	 *  runs: the killer's weapon SILHOUETTE beside its name, the bare COUNTDOWN numeral (the
	 *  design draws the number large and the sentence small), and the respawn RING.
	 *
	 *  The ring is a UProgressBar on purpose. A radial sweep is a MATERIAL — Tier 4 — and this
	 *  ViewModel updates once a second because law 4 forbids the tick that a smooth sweep needs
	 *  from C++. A bar that steps once a second is honest; a ring that stutters is not, so the
	 *  material lands with the art, reading the same fraction. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UImage> WeaponIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> CountdownText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UProgressBar> RespawnRing;

	/** What is on the brush now, so a per-second Refresh never re-issues the same soft load. */
	TSoftObjectPtr<UTexture2D> AppliedWeaponIcon;

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Combat> BoundViewModel;
};
