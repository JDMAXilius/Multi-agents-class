#include "UI/Settings/BRSettingsDataObject.h"

#include "UI/Components/BRComponentTokens.h"

void UBRSettingsDataObject::SetIdentity(FName InId, const FText& InDisplayName, const FText& InDescription)
{
	SettingId = InId;
	DisplayName = InDisplayName;
	Description = InDescription;
}

FText UBRSettingsDataObject::GetDisplayValue() const
{
	// The em dash, not empty. A blank right-hand column next to a populated one reads as "this
	// setting has no value"; the dash reads as "this row does not carry one", which is the true
	// statement for a header or a collection.
	return BRUI::UnknownValueText();
}

void UBRSettingsDataObject::SetEditState(EBRSettingsEditState InState, const FText& InReason)
{
	if (EditState == InState && DisabledReason.EqualTo(InReason))
	{
		return;
	}

	EditState = InState;
	DisabledReason = InState == EBRSettingsEditState::Editable ? FText::GetEmpty() : InReason;

	// An edit-state change is a change: the row has to re-render greyed even though its value
	// did not move. Routing it through the same delegate keeps the list entry's update path
	// singular -- it never has to ask WHICH kind of change happened.
	NotifyChanged();
}

void UBRSettingsDataObject::Refresh()
{
	// Base has nothing to re-read. Deliberately not pure virtual: most leaves have no dependency
	// to re-evaluate either, and forcing every one of them to write an empty override would make
	// the interesting overrides harder to spot.
}

void UBRSettingsDataObject::NotifyChanged()
{
	OnSettingChanged.Broadcast(this);
}

void UBRSettingsCollection::AddChild(UBRSettingsDataObject* Child)
{
	if (!Child)
	{
		return;
	}

	Children.Add(Child);

	ChildrenView.Reset(Children.Num());
	for (const TObjectPtr<UBRSettingsDataObject>& Entry : Children)
	{
		ChildrenView.Add(Entry.Get());
	}
}

void UBRSettingsCollection::FlattenVisible(TArray<UBRSettingsDataObject*>& OutRows) const
{
	for (const TObjectPtr<UBRSettingsDataObject>& Child : Children)
	{
		if (!Child)
		{
			continue;
		}

		// Unsupported is dropped, not greyed. See the header: a row the platform can never
		// service is noise, whereas DependencyUnmet is kept because the player can act on it.
		if (Child->GetEditState() == EBRSettingsEditState::Unsupported)
		{
			continue;
		}

		OutRows.Add(Child);

		if (const UBRSettingsCollection* Nested = Cast<UBRSettingsCollection>(Child))
		{
			// The nested collection itself is the header row; its children follow it inline.
			Nested->FlattenVisible(OutRows);
		}
	}
}

bool UBRSettingsCollection::IsDirty() const
{
	for (const TObjectPtr<UBRSettingsDataObject>& Child : Children)
	{
		if (Child && Child->IsDirty())
		{
			return true;
		}
	}

	return false;
}

void UBRSettingsCollection::ResetToDefault()
{
	for (const TObjectPtr<UBRSettingsDataObject>& Child : Children)
	{
		if (Child)
		{
			Child->ResetToDefault();
		}
	}
}

void UBRSettingsCollection::CaptureBaseline()
{
	for (const TObjectPtr<UBRSettingsDataObject>& Child : Children)
	{
		if (Child)
		{
			Child->CaptureBaseline();
		}
	}
}

void UBRSettingsCollection::RestoreBaseline()
{
	for (const TObjectPtr<UBRSettingsDataObject>& Child : Children)
	{
		if (Child)
		{
			Child->RestoreBaseline();
		}
	}
}

void UBRSettingsCollection::Refresh()
{
	for (const TObjectPtr<UBRSettingsDataObject>& Child : Children)
	{
		if (Child)
		{
			Child->Refresh();
		}
	}
}
