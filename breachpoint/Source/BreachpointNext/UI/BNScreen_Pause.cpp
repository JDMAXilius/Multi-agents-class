#include "UI/BNScreen_Pause.h"
#include "BreachpointNext.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"
#include "Components/TextBlock.h"
#include "Match/BNPlayerController.h"
#include "UI/BNHUDDirector.h"
#include "UI/BNUITypes.h"
#include "Engine/LocalPlayer.h"

#define LOCTEXT_NAMESPACE "BreachpointNextUI"

UBNScreen_Pause::UBNScreen_Pause()
{
	// The only screen in R7 that wants the cursor and the clicks.
	InputMode = EBNWidgetInputMode::Menu;
	// The day a UCommonUIInputData lands ARRIVED (BN43, 2 Sep): CommonInputSettings is wired,
	// so this flag is live and NativeOnHandleBackAction can now fire alongside NativeOnKeyDown.
	// See the header — which of the two wins, and whether both run, is BN46's measurement.
	bIsBackHandler = true;
	// Input routing stops here while the menu is up — the shape both proven modals use.
	bIsModal = true;
}

void UBNScreen_Pause::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Visible, not HitTestInvisible — unlike every other BN screen, this one is FOR clicking.
	SetVisibility(ESlateVisibility::Visible);

	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddUniqueDynamic(this, &UBNScreen_Pause::HandleResumeClicked);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked.AddUniqueDynamic(this, &UBNScreen_Pause::HandleLeaveClicked);
	}

	// Hard binds now, so these cannot be null on a well-formed asset — guarded anyway because a
	// half-built WBP during authoring should log, not crash.
	if (WarningText)
	{
		WarningText->SetText(LOCTEXT("NoPauseTitle", "THE MATCH DOES NOT PAUSE"));
		WarningText->SetColorAndOpacity(FSlateColor(BNUIColors::Threat));
	}
	if (WarningBodyText)
	{
		WarningBodyText->SetText(LOCTEXT("NoPauseBody", "Server-authoritative — this is a menu overlay only."));
		WarningBodyText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
	}
}

UWidget* UBNScreen_Pause::NativeGetDesiredFocusTarget() const
{
	// Resume, always: a controller opening this menu must land on the harmless row, never on
	// the one that leaves the match.
	return ResumeButton ? ResumeButton : Super::NativeGetDesiredFocusTarget();
}

bool UBNScreen_Pause::NativeOnHandleBackAction()
{
	DeactivateWidget();
	return true;
}

FReply UBNScreen_Pause::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		DeactivateWidget();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBNScreen_Pause::NativeOnDeactivated()
{
	// TELL THE DIRECTOR (critic): this screen can close itself three ways, and a director that
	// still believes the menu is open re-opens it the next time it re-evaluates the layer.
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UBNHUDDirector* Director = LocalPlayer->GetSubsystem<UBNHUDDirector>())
		{
			Director->NotifyPauseClosed();
		}
	}

	Super::NativeOnDeactivated();
}

void UBNScreen_Pause::HandleResumeClicked()
{
	DeactivateWidget();
}

void UBNScreen_Pause::HandleLeaveClicked()
{
	// The controller owns leaving — a widget must never travel the player itself.
	//
	// ORDER IS LOAD-BEARING (critic): LeaveMatch only SCHEDULES the travel (SetClientTravel,
	// browsed on the next tick), so deactivating AFTER it restores the game input config before
	// the world is browsed. Reversed, the player travels with Menu input still applied and the
	// widget that would restore it destroyed with the old world.
	if (ABNPlayerController* PC = GetOwningPlayer<ABNPlayerController>())
	{
		PC->LeaveMatch();
	}
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
