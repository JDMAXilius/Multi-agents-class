#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Components/SlateWrapperTypes.h"
#include "OSBaseCommonButton.generated.h"

class UCommonTextBlock;

/*
  Reusable CommonUI design-system button. Inherits UCommonButtonBase so it gets
  the standard CommonUI input/focus/selection handling for free. Derive a
  Blueprint from this and bind a Txt_ButtonName to display ButtonName, or
  override the BP hooks for custom visuals.
 */
UCLASS()
class ONSIGHT_API UOSBaseCommonButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UOSBaseCommonButton(const FObjectInitializer& ObjectInitializer);

	// --- Bound Widgets ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_ButtonName;

	// --- Exposed Properties ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Base Common Button")
	FText ButtonName = FText::FromString(TEXT("ButtonName"));

	// --- Slot Properties (applied by parent container after adding to panel) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Common Button|Slot")
	FMargin SlotPadding = FMargin(0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Common Button|Slot")
	FSlateChildSize SlotSize = FSlateChildSize(ESlateSizeRule::Automatic);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Common Button|Slot")
	TEnumAsByte<EHorizontalAlignment> SlotHorizontalAlignment = HAlign_Fill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Common Button|Slot")
	TEnumAsByte<EVerticalAlignment> SlotVerticalAlignment = VAlign_Fill;

	// --- Label ---

	UFUNCTION(BlueprintCallable, Category = "Base Common Button")
	void SetButtonName(const FText& InName);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- CommonButtonBase Overrides ---

	virtual void NativeOnClicked() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnPressed() override;
	virtual void NativeOnReleased() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;

private:
	void ApplyLabelText();
};
