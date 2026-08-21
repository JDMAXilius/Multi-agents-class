#include "UI/BNActivatableWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UI/BNUIManager.h"
#include "UI/BNViewModels.h"

TOptional<FUIInputConfig> UBNActivatableWidget::GetDesiredInputConfig() const
{
	// Exact shapes from the compiled reference (ROADMAP-7 §API) — the 3-arg FUIInputConfig and
	// the UNSET optional for Inherit (a default-constructed config would be an answer, and
	// Inherit's whole meaning is "no answer, ask below").
	switch (InputMode)
	{
	case EBNWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown, bHideCursorDuringViewportCapture);
	case EBNWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::CaptureDuringMouseDown, bHideCursorDuringViewportCapture);
	case EBNWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
	case EBNWidgetInputMode::Inherit:
	default:
		return TOptional<FUIInputConfig>();
	}
}

void UBNActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	BindViewModels();
}

void UBNActivatableWidget::NativeOnDeactivated()
{
	UnbindViewModels();
	Super::NativeOnDeactivated();
}

UBNVM_Combat* UBNActivatableWidget::GetCombatViewModel() const
{
	const UBNUIManager* Manager = UBNUIManager::Get(this);
	return Manager ? Manager->GetCombatViewModel(GetOwningLocalPlayer()) : nullptr;
}

UBNVM_Match* UBNActivatableWidget::GetMatchViewModel() const
{
	const UBNUIManager* Manager = UBNUIManager::Get(this);
	return Manager ? Manager->GetMatchViewModel(GetOwningLocalPlayer()) : nullptr;
}
