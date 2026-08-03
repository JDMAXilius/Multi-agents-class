#include "UI/Screens/BRModal_Warning.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "UI/Components/BRScrim.h"

UBRModal_Warning::UBRModal_Warning()
{
	// CommonUI's own back handling: back deactivates this widget, the stack releases it and focus
	// returns to the layer beneath. Nothing here implements back itself.
	bIsBackHandler = true;

	// Input routing stops at the modal -- the screen beneath stays active but must not act on
	// anything while a question is on screen.
	bIsModal = true;
}

TOptional<FUIInputConfig> UBRModal_Warning::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void UBRModal_Warning::SetConfirmRequest(const FBRConfirmRequest& InRequest)
{
	Request = InRequest;
	bResolved = false;

	// May be a no-op this early: BindWidget pointers are null until the widget is constructed.
	// NativeOnActivated applies it again.
	ApplyRequest();
}

void UBRModal_Warning::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (Scrim)
	{
		Scrim->SetScrimActive(true);
	}

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &UBRModal_Warning::HandleConfirmClicked);
	}

	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &UBRModal_Warning::HandleCancelClicked);
	}

	ApplyRequest();
}

void UBRModal_Warning::NativeOnDeactivated()
{
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
	}

	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
	}

	if (Scrim)
	{
		Scrim->SetScrimActive(false);
	}

	// Popping IS the cancel path. Whoever popped us -- back, a parent screen tearing the layer
	// down, a travel -- the caller still gets its one answer.
	Resolve(false);

	Super::NativeOnDeactivated();
}

UWidget* UBRModal_Warning::NativeGetDesiredFocusTarget() const
{
	if (CancelButton)
	{
		return CancelButton;
	}

	if (ConfirmButton)
	{
		return ConfirmButton;
	}

	return Super::NativeGetDesiredFocusTarget();
}

void UBRModal_Warning::ApplyRequest()
{
	// An empty string collapses its widget rather than rendering an empty band -- a blank title
	// bar tells the player the screen has something to say when it does not.
	if (TitleText)
	{
		TitleText->SetText(Request.Title);
		TitleText->SetVisibility(Request.Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (BodyText)
	{
		BodyText->SetText(Request.Body);
		BodyText->SetVisibility(Request.Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	// An empty label leaves the WBP's authored default in place; it never blanks a button.
	if (ConfirmLabelText && !Request.ConfirmLabel.IsEmpty())
	{
		ConfirmLabelText->SetText(Request.ConfirmLabel);
	}

	if (CancelLabelText && !Request.CancelLabel.IsEmpty())
	{
		CancelLabelText->SetText(Request.CancelLabel);
	}
}

void UBRModal_Warning::HandleConfirmClicked()
{
	// Resolve BEFORE deactivating: deactivation runs the cancel path, and the guard in Resolve is
	// what keeps a confirmed answer from being overwritten by it.
	Resolve(true);

	DeactivateWidget();
}

void UBRModal_Warning::HandleCancelClicked()
{
	Resolve(false);

	DeactivateWidget();
}

void UBRModal_Warning::Resolve(bool bConfirmed)
{
	if (bResolved)
	{
		return;
	}

	bResolved = true;

	// Move the delegate out before firing: the caller is free to push another modal from inside
	// its own handler, and re-entering this widget must not find a live binding.
	FBRConfirmResolved Resolved = MoveTemp(Request.OnResolved);
	Request.OnResolved.Unbind();

	Resolved.ExecuteIfBound(bConfirmed);
}
