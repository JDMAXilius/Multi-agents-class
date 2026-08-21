#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "BNAmmoBlock.generated.h"

class UBNVM_Combat;
class UTextBlock;

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

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Combat> BoundViewModel;
};
