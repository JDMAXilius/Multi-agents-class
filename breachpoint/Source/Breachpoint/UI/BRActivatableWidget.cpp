// Breachpoint. The one widget base.
#include "UI/BRActivatableWidget.h"

#include "Core/BRCore.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UI/BRUIManagerSubsystem.h"
#include "UI/BRViewModels.h"

TOptional<FUIInputConfig> UBRActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputMode)
	{
	case EBRWidgetInputMode::Game:
		return FUIInputConfig(
			ECommonInputMode::Game,
			EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown,
			bHideCursorDuringViewportCapture);

	case EBRWidgetInputMode::GameAndMenu:
		return FUIInputConfig(
			ECommonInputMode::All,
			EMouseCaptureMode::CaptureDuringMouseDown,
			bHideCursorDuringViewportCapture);

	case EBRWidgetInputMode::Menu:
		return FUIInputConfig(
			ECommonInputMode::Menu,
			EMouseCaptureMode::NoCapture,
			false);

	case EBRWidgetInputMode::Inherit:
	default:
		return TOptional<FUIInputConfig>();
	}
}

UBRUIManagerSubsystem* UBRActivatableWidget::GetUIManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<UBRUIManagerSubsystem>();
}

UBRVM_Combat* UBRActivatableWidget::GetCombatViewModel() const
{
	UBRUIManagerSubsystem* Manager = GetUIManager();
	return Manager ? Manager->GetCombatViewModel(GetOwningLocalPlayer()) : nullptr;
}

UBRVM_Match* UBRActivatableWidget::GetMatchViewModel() const
{
	UBRUIManagerSubsystem* Manager = GetUIManager();
	return Manager ? Manager->GetMatchViewModel(GetOwningLocalPlayer()) : nullptr;
}

void UBRActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	BindViewModels();
}

void UBRActivatableWidget::NativeOnDeactivated()
{
	UnbindViewModels();

	Super::NativeOnDeactivated();
}
