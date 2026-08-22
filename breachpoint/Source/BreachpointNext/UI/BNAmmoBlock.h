#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "BNAmmoBlock.generated.h"

class UBNVM_Combat;
class UImage;
class UTextBlock;
class UTexture2D;

/**
 * Bottom-right: the weapon's name, the magazine, the reserve. INDEX_NONE from the VM means "no
 * magazine" (the knife, the empty hand) and renders a dash — this block never invents a zero.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNAmmoBlock : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void BindCombatField(UBNVM_Combat* Combat, UE::FieldNotification::FFieldId FieldId);
	void HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);
	void Refresh();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> WeaponNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UTextBlock> MagAmmoText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> ReserveAmmoText;

	/** The design's 88×32 silhouette slot. Optional: a HUD without it still runs, minus the gun. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UImage> WeaponIcon;

	/** R7.3 — the stowed slot under the rule: the weapon ONE swap press away. Both optional and
	 *  both hidden together when there is nothing to swap to, so a one-weapon carry shows an
	 *  empty rule rather than the gun already in the hand. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> StowedNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UImage> StowedIcon;

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Combat> BoundViewModel;

	/** The icon currently on the brush, so a per-shot Refresh never re-issues the load. */
	TSoftObjectPtr<UTexture2D> AppliedIcon;
	TSoftObjectPtr<UTexture2D> AppliedStowedIcon;
};
