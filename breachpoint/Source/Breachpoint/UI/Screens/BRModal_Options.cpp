#include "UI/Screens/BRModal_Options.h"

#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "UI/Components/BRScrim.h"

DEFINE_LOG_CATEGORY_STATIC(LogBRModalOptions, Log, All);

UBRModal_Options::UBRModal_Options()
{
	// CommonUI's back action deactivates the modal; the stack restores focus beneath. No bespoke
	// back handler (`ue5-ui-architecture` Sec 6).
	bIsBackHandler = true;

	bIsModal = true;
}

TOptional<FUIInputConfig> UBRModal_Options::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void UBRModal_Options::SetOptionsRequest(const FBROptionsRequest& InRequest)
{
	Request = InRequest;

	// No-op before construction (BindWidget pointers are null); NativeOnActivated rebuilds.
	if (RowContainer)
	{
		RebuildRows();
	}
}

void UBRModal_Options::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// SCREEN-MANIFEST Sec 8.2: the modal keeps its 451 x 682 footprint at every aspect. Driving it
	// from C++ is what stops it being stretched to 21:9 from a details panel.
	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(PopupWidth);
		RootSizeBox->SetHeightOverride(PopupHeight);
	}
}

void UBRModal_Options::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (Scrim)
	{
		Scrim->SetScrimActive(true);
	}

	if (TitleText)
	{
		TitleText->SetText(Request.Title);
		TitleText->SetVisibility(Request.Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	RebuildRows();
}

void UBRModal_Options::NativeOnDeactivated()
{
	// Every row binding made in RebuildRows is dropped here. A row delegate that outlives the
	// modal is the leak `ue5-ui-architecture` Sec 2 names.
	ReleaseRows();

	if (Scrim)
	{
		Scrim->SetScrimActive(false);
	}

	Super::NativeOnDeactivated();
}

UWidget* UBRModal_Options::NativeGetDesiredFocusTarget() const
{
	// Focus the first row that can actually take it -- focusing a disabled row is a dead gamepad.
	for (const TObjectPtr<UBRMenuRow>& RowWidget : RowWidgets)
	{
		if (RowWidget && RowWidget->GetIsEnabled())
		{
			return RowWidget;
		}
	}

	return Super::NativeGetDesiredFocusTarget();
}

void UBRModal_Options::RebuildRows()
{
	if (!RowContainer)
	{
		return;
	}

	ReleaseRows();

	if (Request.Rows.IsEmpty())
	{
		// Honest empty state: no rows means no list, not a bordered box pretending to hold one.
		RowContainer->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// SelfHitTestInvisible: the container lays the rows out, the rows take the input.
	RowContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	UClass* ResolvedRowClass = RowWidgetClass.LoadSynchronous();
	if (!ResolvedRowClass)
	{
		// Silence here would render an empty modal that looks like "no options available".
		UE_LOG(LogBRModalOptions, Warning,
			TEXT("%s: RowWidgetClass is unset or failed to load; %d option row(s) dropped."),
			*GetName(), Request.Rows.Num());
		return;
	}

	RowWidgets.Reserve(Request.Rows.Num());

	for (const FBROptionsRow& Row : Request.Rows)
	{
		UBRMenuRow* RowWidget = CreateWidget<UBRMenuRow>(this, ResolvedRowClass);
		if (!RowWidget)
		{
			continue;
		}

		RowWidget->SetRowType(Row.RowType);
		RowWidget->SetLabelText(Row.Label);
		RowWidget->SetSelectionText(Row.Value);
		RowWidget->SetIsEnabled(Row.bEnabled);

		// Payload-argument binding: the row id travels with the delegate, so no row widget has to
		// know its own index and no index has to be kept in sync with the payload.
		RowWidget->OnClicked().AddUObject(this, &UBRModal_Options::HandleRowClicked, Row.RowId);

		RowContainer->AddChild(RowWidget);
		RowWidgets.Add(RowWidget);
	}
}

void UBRModal_Options::ReleaseRows()
{
	for (const TObjectPtr<UBRMenuRow>& RowWidget : RowWidgets)
	{
		if (RowWidget)
		{
			RowWidget->OnClicked().RemoveAll(this);
		}
	}

	RowWidgets.Reset();

	if (RowContainer)
	{
		RowContainer->ClearChildren();
	}
}

void UBRModal_Options::HandleRowClicked(FName RowId)
{
	Request.OnRowChosen.ExecuteIfBound(RowId);

	// SCREEN-BUILD-SPEC Sec 3.9, the one behavioural difference between the two variants:
	// an action commits and closes; a filter toggles and the panel stays up for the next one.
	// ponytail: the Filters variant does NOT re-render its own rows from the toggle -- the caller
	// owns filter state and re-supplies rows via SetOptionsRequest. Push row state down here only
	// if a caller is ever forced to duplicate that bookkeeping.
	if (Request.Variant == EBRModalOptionsVariant::Actions)
	{
		DeactivateWidget();
	}
}
