#include "UI/Screens/BRScreen_Settings.h"

#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"
#include "Engine/LocalPlayer.h"
#include "UI/BRUIManagerSubsystem.h"
#include "UI/BRUISettings.h"
#include "UI/BRUITypes.h"
#include "UI/Components/BRNavBar.h"
#include "UI/Screens/BRModal_Warning.h"
#include "UI/Screens/BRScreen_KeyRemap.h"
#include "UI/Settings/BRSettingsDataObject.h"
#include "UI/Settings/BRSettingsKeyRemap.h"
#include "UI/Settings/BRSettingsRegistry.h"

#define LOCTEXT_NAMESPACE "BRSettings"

DEFINE_LOG_CATEGORY_STATIC(LogBRScreenSettings, Log, All);

UBRScreen_Settings::UBRScreen_Settings()
{
	// A settings screen is a full navigation level: it takes exclusive Menu input and IS the back
	// handler while it is on top. Both are declared in C++, not left to a WBP details panel, for
	// the same reason `UBRModal_Warning` declares its own -- a screen whose input mode depends on
	// an asset default is a screen that behaves differently depending on which WBP opened it.
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> UBRScreen_Settings::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UBRScreen_Settings::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (TitleText)
	{
		TitleText->SetText(LOCTEXT("Screen_SettingsTitle", "Settings"));
	}
}

void UBRScreen_Settings::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (!Registry)
	{
		Registry = NewObject<UBRSettingsRegistry>(this);
	}

	// Built on every activation, not once: the local player can change between openings (PIE
	// with two windows, a controller joining), and the key-binding rows are read out of THAT
	// player's Enhanced Input profile.
	Registry->Build(GetOwningLocalPlayer());
	Registry->OnAnySettingChanged.AddUObject(this, &UBRScreen_Settings::HandleAnySettingChanged);

	TabIds.Reset();

	TArray<FBRNavTabDefinition> TabDefinitions;
	for (const UBRSettingsCollection* Tab : Registry->GetTabs())
	{
		if (!Tab)
		{
			continue;
		}

		FBRNavTabDefinition Definition;
		Definition.Label = Tab->GetDisplayName();
		TabDefinitions.Add(MoveTemp(Definition));

		// The parallel array is what maps a nav-bar INDEX back to a registry tab id. The nav bar
		// is deliberately ignorant of settings, so something has to hold the correspondence, and
		// holding it here keeps `UBRNavBar` reusable by screens that have no registry at all.
		TabIds.Add(Tab->GetSettingId());
	}

	if (TabBar)
	{
		TabBar->SetTabs(TabDefinitions);

		// AddDynamic, not AddUObject: OnTabSelected is a DYNAMIC multicast delegate. AddUObject
		// does not compile against one, and the reflected handler is why HandleTabSelected
		// carries a UFUNCTION.
		TabBar->OnTabSelected.AddDynamic(this, &UBRScreen_Settings::HandleTabSelected);
		TabBar->SetSelectedTabIndex(0);
	}

	// SetSelectedTabIndex may or may not have broadcast, depending on whether index 0 was already
	// selected. Selecting the first tab explicitly here makes the initial state independent of
	// that -- a screen that renders empty on first open because a delegate did not fire is a
	// genuinely hard bug to find twice.
	if (TabIds.Num() > 0)
	{
		SelectedTabId = TabIds[0];
	}

	RebuildRows();
}

void UBRScreen_Settings::NativeOnDeactivated()
{
	if (TabBar)
	{
		TabBar->OnTabSelected.RemoveDynamic(this, &UBRScreen_Settings::HandleTabSelected);
	}

	if (Registry)
	{
		Registry->OnAnySettingChanged.RemoveAll(this);
	}

	ReleaseRows();
	bConfirmPending = false;

	Super::NativeOnDeactivated();
}

UWidget* UBRScreen_Settings::NativeGetDesiredFocusTarget() const
{
	// First row, so the gamepad lands somewhere real and CommonUI navigates from there. Falls
	// back to the tab bar when a tab is empty -- which the Controls tab is in any build where
	// Enhanced Input user settings are off.
	for (const TObjectPtr<UBRSettingsRow>& Row : RowWidgets)
	{
		if (Row && Row->GetIsEnabled())
		{
			return Row.Get();
		}
	}

	return TabBar;
}

void UBRScreen_Settings::HandleTabSelected(int32 TabIndex)
{
	if (!TabIds.IsValidIndex(TabIndex))
	{
		return;
	}

	SelectedTabId = TabIds[TabIndex];
	RebuildRows();
}

/**
 * Load a soft row class, or fall back. Never returns a class the caller did not ask for without
 * saying so: a null soft pointer is a deliberate "use the default", while a set-but-unloadable
 * one is a config error worth a line in the log.
 */
static UClass* ResolveRowClass(const TSoftClassPtr<UBRSettingsRow>& SoftClass, UClass* Fallback)
{
	if (SoftClass.IsNull())
	{
		return Fallback;
	}

	if (UClass* Loaded = SoftClass.LoadSynchronous())
	{
		return Loaded;
	}

	UE_LOG(LogBRScreenSettings, Warning,
		TEXT("Settings row class '%s' is set but failed to load; falling back."),
		*SoftClass.ToString());
	return Fallback;
}

void UBRScreen_Settings::RebuildRows()
{
	ReleaseRows();

	if (!Registry || !RowContainer || SelectedTabId.IsNone())
	{
		return;
	}

	// Law 3: SOFT class, resolved here from config. There is no hard widget-class pointer and no
	// constructor asset lookup anywhere in this file.
	const UBRUISettings& UISettings = UBRUISettings::Get();
	UClass* DefaultRowClass = ResolveRowClass(UISettings.SettingsRowClass, nullptr);
	if (!DefaultRowClass)
	{
		UE_LOG(LogBRScreenSettings, Warning,
			TEXT("UBRUISettings::SettingsRowClass is unset or failed to load -- the settings list ")
			TEXT("will render empty. Set it in Config/DefaultGame.ini."));
		return;
	}

	TArray<UBRSettingsDataObject*> Rows;
	Registry->GetRowsForTab(SelectedTabId, Rows);

	for (UBRSettingsDataObject* Setting : Rows)
	{
		// The TYPE picks the CLASS, because the per-type body lives in the asset. Resolved from
		// the setting rather than from a built row -- see UBRSettingsRow::ResolveButtonTypeFor.
		// Every branch falls back to the default row, so an unset entry degrades to a plain
		// label-and-value row rather than to a gap in the list.
		UClass* RowClass = DefaultRowClass;
		switch (UBRSettingsRow::ResolveButtonTypeFor(Setting))
		{
		case EBRButtonType::Slider:
			RowClass = ResolveRowClass(UISettings.SettingsRowSliderClass, DefaultRowClass);
			break;

		case EBRButtonType::Checkbox:
			RowClass = ResolveRowClass(UISettings.SettingsRowCheckboxClass, DefaultRowClass);
			break;

		case EBRButtonType::DropDown:
			RowClass = ResolveRowClass(UISettings.SettingsRowDropDownClass, DefaultRowClass);
			break;

		default:
			break;
		}

		UBRSettingsRow* Row = CreateWidget<UBRSettingsRow>(this, RowClass);
		if (!Row)
		{
			continue;
		}

		Row->SetSetting(Setting);
		Row->OnSettingRowActivated.BindUObject(this, &UBRScreen_Settings::HandleRowActivated);

		RowContainer->AddChild(Row);
		RowWidgets.Add(Row);
	}

	// Show the first row's description so the panel is not blank before anything takes focus.
	if (Rows.Num() > 0)
	{
		ShowDescriptionFor(Rows[0]);
	}
}

void UBRScreen_Settings::ReleaseRows()
{
	for (const TObjectPtr<UBRSettingsRow>& Row : RowWidgets)
	{
		if (Row)
		{
			Row->OnSettingRowActivated.Unbind();
			Row->RemoveFromParent();
		}
	}

	RowWidgets.Reset();

	if (RowContainer)
	{
		RowContainer->ClearChildren();
	}
}

void UBRScreen_Settings::ShowDescriptionFor(const UBRSettingsDataObject* Setting)
{
	if (!DescriptionText)
	{
		return;
	}

	if (!Setting)
	{
		DescriptionText->SetText(FText::GetEmpty());
		return;
	}

	// A disabled row's REASON outranks its description: "V-Sync is setting the frame rate" is
	// what the player needs while the row is greyed, and the generic blurb is not.
	const FText Reason = Setting->GetDisabledReason();
	DescriptionText->SetText(Reason.IsEmpty() ? Setting->GetDescription() : Reason);
}

void UBRScreen_Settings::HandleAnySettingChanged(UBRSettingsDataObject* Changed)
{
	// The rows redraw themselves from their own per-object delegate. What the SCREEN owes is the
	// description panel, because an edit-state change rewrites the text the panel is showing.
	ShowDescriptionFor(Changed);
}

void UBRScreen_Settings::HandleRowActivated(UBRSettingsDataObject* Setting)
{
	UBRSettingsValue_KeyRemap* Binding = Cast<UBRSettingsValue_KeyRemap>(Setting);
	if (!Binding)
	{
		return;
	}

	UBRUIManagerSubsystem* UIManager = GetUIManager();
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();

	const TSoftClassPtr<UBRActivatableWidget>& CaptureClass = UBRUISettings::Get().KeyRemapScreenClass;
	if (!UIManager || !LocalPlayer || CaptureClass.IsNull())
	{
		return;
	}

	UBRActivatableWidget* Pushed = UIManager->PushWidgetToLayer(
		LocalPlayer, FBRUITags::Get().Layer_Modal, CaptureClass);

	if (UBRScreen_KeyRemap* Capture = Cast<UBRScreen_KeyRemap>(Pushed))
	{
		Capture->SetTargetBinding(Binding);
	}
}

bool UBRScreen_Settings::NativeOnHandleBackAction()
{
	if (bConfirmPending)
	{
		// A confirm is already up. Swallow the press rather than stacking a second modal or
		// popping the screen out from under the question we just asked.
		return true;
	}

	if (!Registry || !Registry->HasUnsavedChanges())
	{
		PopSelf();
		return true;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UBRUIManagerSubsystem* UIManager = GetUIManager();

	const TSoftClassPtr<UBRActivatableWidget>& ConfirmClass = UBRUISettings::Get().ConfirmModalClass;
	if (!UIManager || !LocalPlayer || ConfirmClass.IsNull())
	{
		// No confirm available. Discard rather than trapping the player on the screen -- and say
		// so, because silently dropping their edits with no prompt is a defect either way and
		// this log is the only trace it left.
		UE_LOG(LogBRScreenSettings, Warning,
			TEXT("Backing out of settings with unsaved changes and no UBRUISettings::ConfirmModalClass ")
			TEXT("set. Changes discarded."));

		Registry->CancelChanges();
		PopSelf();
		return true;
	}

	UBRActivatableWidget* Pushed = UIManager->PushWidgetToLayer(
		LocalPlayer, FBRUITags::Get().Layer_Modal, ConfirmClass);

	UBRModal_Warning* Confirm = Cast<UBRModal_Warning>(Pushed);
	if (!Confirm)
	{
		Registry->CancelChanges();
		PopSelf();
		return true;
	}

	bConfirmPending = true;

	FBRConfirmRequest Request;
	Request.Title = LOCTEXT("Discard_Title", "Discard Changes");
	Request.Body = LOCTEXT("Discard_Body", "You have unsaved settings. Leaving now will put them back the way they were.");
	Request.ConfirmLabel = LOCTEXT("Discard_Confirm", "Discard");
	Request.CancelLabel = LOCTEXT("Discard_Cancel", "Keep Editing");

	// CreateUObject, not a reflected binding: the delegate dies with this screen, which is what
	// keeps a confirm resolving into a widget that has already been popped from being possible.
	Request.OnResolved = FBRConfirmResolved::CreateUObject(this,
		&UBRScreen_Settings::HandleDiscardConfirmResolved);

	Confirm->SetConfirmRequest(Request);
	return true;
}

void UBRScreen_Settings::HandleDiscardConfirmResolved(bool bConfirmed)
{
	bConfirmPending = false;

	if (!bConfirmed)
	{
		// "Keep Editing". Stay exactly where we are, with the edits intact.
		return;
	}

	if (Registry)
	{
		Registry->CancelChanges();
	}

	PopSelf();
}

void UBRScreen_Settings::PopSelf()
{
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
