#include "UI/Screens/BRScreen_KeyRemap.h"

#include "CommonTextBlock.h"
#include "Engine/LocalPlayer.h"
#include "UI/BRUIManagerSubsystem.h"
#include "UI/BRUISettings.h"
#include "UI/BRUITypes.h"
#include "UI/Screens/BRModal_Warning.h"
#include "UI/Settings/BRSettingsKeyRemap.h"

#define LOCTEXT_NAMESPACE "BRSettings"

DEFINE_LOG_CATEGORY_STATIC(LogBRScreenKeyRemap, Log, All);

UBRScreen_KeyRemap::UBRScreen_KeyRemap()
{
	// NOT a back handler. Escape is consumed by NativeOnKeyDown as the cancel gesture, and
	// claiming the back action as well would give one press two owners.
	bIsBackHandler = false;
}

TOptional<FUIInputConfig> UBRScreen_KeyRemap::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UBRScreen_KeyRemap::SetTargetBinding(UBRSettingsValue_KeyRemap* InBinding)
{
	Binding = InBinding;
	UpdatePromptText();
}

void UBRScreen_KeyRemap::NativeOnActivated()
{
	Super::NativeOnActivated();

	PendingKey = FKey();
	bConfirmPending = false;

	UpdatePromptText();

	// Focus must land HERE or the key events go to whatever had focus before, and the capture
	// screen sits open receiving nothing while the player's presses drive the settings list
	// underneath it.
	SetKeyboardFocus();
}

void UBRScreen_KeyRemap::UpdatePromptText()
{
	if (ActionText)
	{
		ActionText->SetText(Binding ? Binding->GetDisplayName() : FText::GetEmpty());
	}

	if (PromptText)
	{
		PromptText->SetText(LOCTEXT("KeyRemap_Prompt", "Press a key to bind. Escape cancels."));
	}
}

FReply UBRScreen_KeyRemap::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (bConfirmPending)
	{
		// The confirm owns input right now. Still Handled -- see the class doc on swallowing.
		return FReply::Handled();
	}

	if (Key == EKeys::Escape)
	{
		PopSelf();
		return FReply::Handled();
	}

	// Repeats would fire AttemptBind once per repeat interval while a key is held, stacking
	// conflict confirms behind each other.
	if (!InKeyEvent.IsRepeat())
	{
		AttemptBind(Key);
	}

	// Handled for EVERY key, including ones we rejected. This is the swallow described in the
	// class doc and it is deliberately unconditional.
	return FReply::Handled();
}

FReply UBRScreen_KeyRemap::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bConfirmPending)
	{
		return FReply::Handled();
	}

	// Mouse buttons are legitimate binds in a shooter. Effective key is what carries
	// LeftMouseButton / ThumbMouseButton etc.
	AttemptBind(InMouseEvent.GetEffectingButton());
	return FReply::Handled();
}

void UBRScreen_KeyRemap::AttemptBind(const FKey& InKey)
{
	if (!Binding)
	{
		PopSelf();
		return;
	}

	if (!UBRSettingsValue_KeyRemap::IsBindableKey(InKey))
	{
		// Stay open and say why. Popping on a rejected key would read as "the screen crashed".
		if (PromptText)
		{
			PromptText->SetText(LOCTEXT("KeyRemap_Rejected", "That key cannot be bound. Press another, or Escape to cancel."));
		}

		return;
	}

	TArray<FName> Conflicts;
	const EBRKeyRebindResult Result = Binding->TryAssignKey(InKey, /* bStealFromConflicts */ false, Conflicts);

	switch (Result)
	{
	case EBRKeyRebindResult::Applied:
		PopSelf();
		return;

	case EBRKeyRebindResult::Rejected:
		if (PromptText)
		{
			PromptText->SetText(LOCTEXT("KeyRemap_Refused", "That key was refused. Press another, or Escape to cancel."));
		}
		return;

	case EBRKeyRebindResult::Unavailable:
		UE_LOG(LogBRScreenKeyRemap, Warning,
			TEXT("Rebind unavailable for '%s' -- no Enhanced Input user settings."), *Binding->GetMappingName().ToString());
		PopSelf();
		return;

	case EBRKeyRebindResult::Conflict:
	default:
		break;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UBRUIManagerSubsystem* UIManager = GetUIManager();

	const TSoftClassPtr<UBRActivatableWidget>& ConfirmClass = UBRUISettings::Get().ConfirmModalClass;
	if (!UIManager || !LocalPlayer || ConfirmClass.IsNull())
	{
		// No way to ask. REFUSE rather than steal: silently unbinding an action the player did
		// not mention is the more destructive of the two guesses, and it is the one they cannot
		// see happen.
		if (PromptText)
		{
			PromptText->SetText(LOCTEXT("KeyRemap_ConflictNoPrompt", "That key is already in use. Press another, or Escape to cancel."));
		}

		return;
	}

	UBRActivatableWidget* Pushed = UIManager->PushWidgetToLayer(
		LocalPlayer, FBRUITags::Get().Layer_Modal, ConfirmClass);

	UBRModal_Warning* Confirm = Cast<UBRModal_Warning>(Pushed);
	if (!Confirm)
	{
		return;
	}

	PendingKey = InKey;
	bConfirmPending = true;

	FString ConflictNames;
	for (const FName& Name : Conflicts)
	{
		if (!ConflictNames.IsEmpty())
		{
			ConflictNames += TEXT(", ");
		}

		ConflictNames += Name.ToString();
	}

	FBRConfirmRequest Request;
	Request.Title = LOCTEXT("Steal_Title", "Key Already Bound");
	Request.Body = FText::Format(
		LOCTEXT("Steal_Body", "{0} is already bound to {1}. Rebind it anyway?"),
		InKey.GetDisplayName(), FText::FromString(ConflictNames));
	Request.ConfirmLabel = LOCTEXT("Steal_Confirm", "Rebind");
	Request.CancelLabel = LOCTEXT("Steal_Cancel", "Cancel");
	Request.OnResolved = FBRConfirmResolved::CreateUObject(this, &UBRScreen_KeyRemap::HandleStealConfirmResolved);

	Confirm->SetConfirmRequest(Request);
}

void UBRScreen_KeyRemap::HandleStealConfirmResolved(bool bConfirmed)
{
	bConfirmPending = false;

	if (!bConfirmed || !Binding || !PendingKey.IsValid())
	{
		PendingKey = FKey();

		// Stay open on a cancel so the player can pick a different key without reopening.
		UpdatePromptText();
		SetKeyboardFocus();
		return;
	}

	TArray<FName> Conflicts;
	Binding->TryAssignKey(PendingKey, /* bStealFromConflicts */ true, Conflicts);

	PendingKey = FKey();
	PopSelf();
}

void UBRScreen_KeyRemap::PopSelf()
{
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
