#include "UI/Screens/BRPanel_Toast.h"

#include "CommonTextBlock.h"
#include "Components/SizeBox.h"
#include "Engine/World.h"
#include "TimerManager.h"

UBRPanel_Toast::UBRPanel_Toast()
{
	// A toast is information, not a destination: it must not enter focus routing or change the
	// input config of the screen it floats over.
	bSupportsActivationFocus = false;
}

void UBRPanel_Toast::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// SCREEN-MANIFEST Sec 1: 349 is column 1 exactly. Driven from C++ so it stays on the grid.
	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(ToastWidth);
		RootSizeBox->SetHeightOverride(ToastHeight);
	}
}

void UBRPanel_Toast::SetToastMessage(const FText& InMessage, float InDisplaySeconds)
{
	Message = InMessage;
	DisplaySeconds = InDisplaySeconds;

	if (MessageText)
	{
		MessageText->SetText(Message);
	}

	// Replacing the message on a live toast restarts the dwell rather than inheriting what is left
	// of the previous one.
	if (IsActivated())
	{
		StartDismissTimer();
	}
}

void UBRPanel_Toast::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Applied here as well as in the setter: BindWidget pointers are null until construction, and
	// a toast is routinely configured on the frame it is pushed.
	if (MessageText)
	{
		MessageText->SetText(Message);
	}

	StartDismissTimer();
}

void UBRPanel_Toast::NativeOnDeactivated()
{
	// The timer is the only binding this widget owns, and it does not outlive it.
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimerHandle);
	}

	DismissTimerHandle.Invalidate();

	Super::NativeOnDeactivated();
}

void UBRPanel_Toast::StartDismissTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Duration = DisplaySeconds > 0.0f ? DisplaySeconds : DefaultDisplaySeconds;
	if (Duration <= 0.0f)
	{
		// A non-positive duration means "stay up until popped". Say so by leaving no timer,
		// rather than by firing one on the next frame.
		World->GetTimerManager().ClearTimer(DismissTimerHandle);
		return;
	}

	// SetTimer on a handle that is already running restarts it -- no stacked dismissals.
	World->GetTimerManager().SetTimer(DismissTimerHandle, this, &UBRPanel_Toast::HandleDismissTimer, Duration, false);
}

void UBRPanel_Toast::HandleDismissTimer()
{
	// Deactivating is what pops it off the layer -- the toast does not remove itself from a parent
	// or toggle its own visibility. The stack is the only navigation spine.
	DeactivateWidget();
}
