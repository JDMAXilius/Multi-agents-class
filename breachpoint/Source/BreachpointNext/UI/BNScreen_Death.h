#pragma once

#include "FieldNotificationId.h"
#include "UI/BNActivatableWidget.h"
#include "BNScreen_Death.generated.h"

class UBNVM_Combat;
class UTextBlock;

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

	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;
	TWeakObjectPtr<UBNVM_Combat> BoundViewModel;
};
