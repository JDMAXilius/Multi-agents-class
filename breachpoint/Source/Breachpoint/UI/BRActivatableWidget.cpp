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
		// The HUD. Game gets everything; the mouse stays captured so look input is not eaten.
		return FUIInputConfig(
			ECommonInputMode::Game,
			EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown,
			bHideCursorDuringViewportCapture);

	case EBRWidgetInputMode::GameAndMenu:
		// Death overlay / scoreboard / carnage report. ECommonInputMode::All - NOT "GameAndMenu",
		// which is not an enumerator in UE 5.8 (see the enum's comment in the header).
		return FUIInputConfig(
			ECommonInputMode::All,
			EMouseCaptureMode::CaptureDuringMouseDown,
			bHideCursorDuringViewportCapture);

	case EBRWidgetInputMode::Menu:
		// Front end and modals. Nothing reaches the pawn.
		return FUIInputConfig(
			ECommonInputMode::Menu,
			EMouseCaptureMode::NoCapture,
			/*bHideCursorDuringViewportCapture=*/false);

	case EBRWidgetInputMode::Inherit:
	default:
		// Unset TOptional == "I have no opinion". CommonUI keeps the config the widget below
		// established. Returning a config here instead would make every panel fight the screen
		// that owns it.
		return TOptional<FUIInputConfig>();
	}
}

UBRUIManagerSubsystem* UBRActivatableWidget::GetUIManager() const
{
	// Defensive at every hop. During teardown and during travel each of these can be null, and a
	// HUD that crashes on map change is a worse bug than a HUD that renders nothing for a frame.
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

	// Base first, then bind: the base establishes the activation-tree node this widget's focus
	// and input routing hang from, and a subclass that binds before that has a ViewModel feeding
	// a widget CommonUI does not yet consider active.
	BindViewModels();

	UE_LOG(LogBRUI, Verbose, TEXT("Activated %s (input mode %d)"), *GetName(), static_cast<int32>(InputMode));
}

void UBRActivatableWidget::NativeOnDeactivated()
{
	// Unbind BEFORE the base tears the node down. Order matters in exactly one direction: a
	// broadcast that lands on a half-destructed widget is the death-overlay crash.
	UnbindViewModels();

	UE_LOG(LogBRUI, Verbose, TEXT("Deactivated %s"), *GetName());

	Super::NativeOnDeactivated();
}
