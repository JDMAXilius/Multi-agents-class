#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "BNVitalsWidget.generated.h"

class UBNVM_Combat;
class UProgressBar;
class UTextBlock;

/**
 * Top-centre: shield over health. Two rules with teeth:
 *  - Health is HIDDEN UNTIL DAMAGED (the inherited ruling): a full health bar is silence, a
 *    visible one is information. The gate hides the WHOLE bar element, not just its fill —
 *    the old module's open defect was a hidden fill inside a still-drawn frame.
 *  - Colors are the tokens' and only the tokens': shield cyan (you), health yellow (never
 *    green). Unknown renders dim, at zero fill, with dashes — never a confident 0/100.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNVitalsWidget : public UCommonUserWidget
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
	TObjectPtr<UProgressBar> ShieldBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|HUD")
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> ShieldText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|HUD")
	TObjectPtr<UTextBlock> HealthText;

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Combat> BoundViewModel;
};
