#include "UI/Components/BRSettingsRow.h"

#include "UI/Settings/BRSettingsDataObject.h"
#include "UI/Settings/BRSettingsKeyRemap.h"
#include "UI/Settings/BRSettingsValueObjects.h"

void UBRSettingsRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RefreshFromSetting();
}

void UBRSettingsRow::NativeDestruct()
{
	// A row torn down while still subscribed leaves the registry broadcasting into a widget the
	// viewport has already dropped. Unsubscribed here rather than relying on the delegate's weak
	// object check, because "it happens to be safe" is not the same as "nothing is listening".
	UnsubscribeFromSetting();

	Super::NativeDestruct();
}

void UBRSettingsRow::SetSetting(UBRSettingsDataObject* InSetting)
{
	if (Setting == InSetting)
	{
		return;
	}

	UnsubscribeFromSetting();

	Setting = InSetting;

	if (Setting)
	{
		Setting->OnSettingChanged.AddUObject(this, &UBRSettingsRow::HandleSettingChanged);
	}

	RefreshFromSetting();
}

void UBRSettingsRow::UnsubscribeFromSetting()
{
	if (Setting)
	{
		Setting->OnSettingChanged.RemoveAll(this);
	}
}

void UBRSettingsRow::HandleSettingChanged(UBRSettingsDataObject* Changed)
{
	// The delegate is per-object, so this only ever fires for our own setting. Refreshing
	// unconditionally rather than diffing: the row has four things on it and redrawing all of
	// them is cheaper than deciding which one moved.
	RefreshFromSetting();
}

EBRMenuRowType UBRSettingsRow::ResolveRowType() const
{
	return ResolveRowTypeFor(Setting);
}

EBRMenuRowType UBRSettingsRow::ResolveRowTypeFor(const UBRSettingsDataObject* Setting)
{
	if (!Setting)
	{
		return EBRMenuRowType::Default;
	}

	// A collection IS the section header. It carries no value and must not look pressable.
	if (Setting->IsA<UBRSettingsCollection>())
	{
		return EBRMenuRowType::Default;
	}

	if (Setting->IsA<UBRSettingsValue_Scalar>())
	{
		return EBRMenuRowType::Slider;
	}

	if (const UBRSettingsValue_Discrete* Discrete = Cast<UBRSettingsValue_Discrete>(Setting))
	{
		// Two options is a toggle and reads as a checkbox; more is a list and reads as a
		// dropdown. This is the ONLY place the "a bool is a discrete with two options" decision
		// becomes visible to the player, which is the right place for it to surface.
		return Discrete->GetOptions().Num() == 2 ? EBRMenuRowType::Checkbox : EBRMenuRowType::DropDown;
	}

	// Key bindings render as a plain row: the value is the key name and the interaction is a
	// click, not a cycle.
	return EBRMenuRowType::Default;
}

void UBRSettingsRow::RefreshFromSetting()
{
	if (!Setting)
	{
		SetLabelText(FText::GetEmpty());
		SetSelectionText(FText::GetEmpty());
		return;
	}

	SetLabelText(Setting->GetDisplayName());
	SetRowType(ResolveRowType());

	const bool bIsHeader = Setting->IsA<UBRSettingsCollection>();

	// A header shows no value at all -- not the em dash. The dash means "not known yet", and a
	// section title is not a value that is pending.
	SetSelectionText(bIsHeader ? FText::GetEmpty() : Setting->GetDisplayValue());

	// CommonUI owns the disabled treatment; `UBRMenuRow` already dims to 0.5 on it. A header is
	// non-interactive for the same call, which also keeps it out of the gamepad focus chain.
	SetIsInteractionEnabled(!bIsHeader && Setting->IsEditable());
	SetIsSelectable(!bIsHeader);
}

void UBRSettingsRow::NudgeValue(int32 Direction)
{
	if (!Setting || Direction == 0 || !Setting->IsEditable())
	{
		return;
	}

	if (UBRSettingsValue_Scalar* Scalar = Cast<UBRSettingsValue_Scalar>(Setting))
	{
		Scalar->StepBy(Direction);
		return;
	}

	if (UBRSettingsValue_Discrete* Discrete = Cast<UBRSettingsValue_Discrete>(Setting))
	{
		Discrete->CycleBy(Direction);
	}

	// Everything else -- headers, key bindings -- deliberately ignores a nudge. See the header.
}

void UBRSettingsRow::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (!Setting || !Setting->IsEditable())
	{
		return;
	}

	// A discrete row advances on click, which is what a player expects from clicking a toggle.
	// Everything else is handed up: the screen is what knows that a key binding opens a capture
	// modal, and a row that pushed its own screen would be a widget owning navigation.
	if (UBRSettingsValue_Discrete* Discrete = Cast<UBRSettingsValue_Discrete>(Setting))
	{
		Discrete->CycleBy(1);
		return;
	}

	OnSettingRowActivated.ExecuteIfBound(Setting);
}
