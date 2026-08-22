#include "UI/BNScreen_Pause.h"
#include "BreachpointNext.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Match/BNPlayerController.h"
#include "UI/BNUITypes.h"

#define LOCTEXT_NAMESPACE "BreachpointNextUI"

UBNScreen_Pause::UBNScreen_Pause()
{
	// The only screen in R7 that wants the cursor and the clicks.
	InputMode = EBNWidgetInputMode::Menu;
	// CommonUI routes Esc / gamepad B here; without it the menu can only be closed by mouse.
	bIsBackHandler = true;
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

void UBNScreen_Pause::HandleResumeClicked()
{
	DeactivateWidget();
}

void UBNScreen_Pause::HandleLeaveClicked()
{
	// The controller owns leaving — a widget must never travel the player itself.
	if (ABNPlayerController* PC = GetOwningPlayer<ABNPlayerController>())
	{
		PC->LeaveMatch();
	}
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
