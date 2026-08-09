#include "UI/Components/OSBaseCommonButton.h"

#include "CommonTextBlock.h"
#include "OSLogCategories.h"

UOSBaseCommonButton::UOSBaseCommonButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::ctor"));
	SetIsFocusable(true);
}

void UOSBaseCommonButton::NativePreConstruct()
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::NativePreConstruct - %s"), *ButtonName.ToString());
	Super::NativePreConstruct();
	ApplyLabelText();
}

void UOSBaseCommonButton::NativeConstruct()
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::NativeConstruct - %s"), *ButtonName.ToString());
	Super::NativeConstruct();
}

void UOSBaseCommonButton::NativeDestruct()
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::NativeDestruct - %s"), *ButtonName.ToString());
	Super::NativeDestruct();
}

// --- CommonButtonBase Overrides ---

void UOSBaseCommonButton::NativeOnClicked()
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::NativeOnClicked - %s"), *ButtonName.ToString());
	Super::NativeOnClicked();
}

void UOSBaseCommonButton::NativeOnHovered()
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::NativeOnHovered - %s"), *ButtonName.ToString());
	Super::NativeOnHovered();
}

void UOSBaseCommonButton::NativeOnUnhovered()
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::NativeOnUnhovered - %s"), *ButtonName.ToString());
	Super::NativeOnUnhovered();
}

void UOSBaseCommonButton::NativeOnPressed()
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::NativeOnPressed - %s"), *ButtonName.ToString());
	Super::NativeOnPressed();
}

void UOSBaseCommonButton::NativeOnReleased()
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::NativeOnReleased - %s"), *ButtonName.ToString());
	Super::NativeOnReleased();
}

void UOSBaseCommonButton::NativeOnSelected(bool bBroadcast)
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::NativeOnSelected - %s (bBroadcast=%d)"), *ButtonName.ToString(), bBroadcast);
	Super::NativeOnSelected(bBroadcast);
}

void UOSBaseCommonButton::NativeOnDeselected(bool bBroadcast)
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::NativeOnDeselected - %s (bBroadcast=%d)"), *ButtonName.ToString(), bBroadcast);
	Super::NativeOnDeselected(bBroadcast);
}

// --- Label ---

void UOSBaseCommonButton::SetButtonName(const FText& InName)
{
	UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::SetButtonName - %s"), *InName.ToString());
	ButtonName = InName;
	ApplyLabelText();
}

void UOSBaseCommonButton::ApplyLabelText()
{
	if (!Txt_ButtonName)
	{
		UE_LOG(LogOSDebug, Verbose, TEXT("UOSBaseCommonButton::ApplyLabelText - no Txt_ButtonName bound"));
		return;
	}
	Txt_ButtonName->SetText(ButtonName);
}
