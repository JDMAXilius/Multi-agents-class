#include "UI/Settings/BRSettingsKeyRemap.h"

#include "GameplayTagContainer.h"
#include "UI/Components/BRComponentTokens.h"

void UBRSettingsValue_KeyRemap::Initialize(UEnhancedInputUserSettings* InSettings, FName InMappingName, EPlayerMappableKeySlot InSlot)
{
	UserSettings = InSettings;
	MappingName = InMappingName;
	Slot = InSlot;

	BaselineKey = GetCurrentKey();
}

bool UBRSettingsValue_KeyRemap::IsBound() const
{
	return UserSettings.IsValid() && !MappingName.IsNone();
}

const FPlayerKeyMapping* UBRSettingsValue_KeyRemap::FindMapping() const
{
	if (!IsBound())
	{
		return nullptr;
	}

	const UEnhancedPlayerMappableKeyProfile* Profile = UserSettings->GetActiveKeyProfile();
	if (!Profile)
	{
		return nullptr;
	}

	const FKeyMappingRow* Row = Profile->FindKeyMappingRow(MappingName);
	if (!Row)
	{
		return nullptr;
	}

	// A row holds every slot's mapping for this action; pick ours out by slot rather than
	// assuming an order, because the set is unordered by construction.
	for (const FPlayerKeyMapping& Mapping : Row->Mappings)
	{
		if (Mapping.GetSlot() == Slot)
		{
			return &Mapping;
		}
	}

	return nullptr;
}

FMapPlayerKeyArgs UBRSettingsValue_KeyRemap::MakeArgs(const FKey& InKey) const
{
	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.Slot = Slot;
	Args.NewKey = InKey;

	// Empty profile id means "the equipped profile", which is what a settings screen always
	// wants -- naming one here would edit a profile the player is not looking at.
	Args.ProfileIdString = FString();
	Args.bCreateMatchingSlotIfNeeded = true;

	return Args;
}

FKey UBRSettingsValue_KeyRemap::GetCurrentKey() const
{
	const FPlayerKeyMapping* Mapping = FindMapping();
	return Mapping ? Mapping->GetCurrentKey() : FKey();
}

FKey UBRSettingsValue_KeyRemap::GetDefaultKey() const
{
	const FPlayerKeyMapping* Mapping = FindMapping();
	return Mapping ? Mapping->GetDefaultKey() : FKey();
}

bool UBRSettingsValue_KeyRemap::IsBindableKey(const FKey& InKey)
{
	if (!InKey.IsValid() || InKey == EKeys::AnyKey)
	{
		return false;
	}

	// Escape is reserved for cancelling the capture. See the header for why this one is not
	// negotiable rather than merely discouraged.
	if (InKey == EKeys::Escape)
	{
		return false;
	}

	// Gestures and continuous axes cannot express a discrete press. Mouse buttons and gamepad
	// face buttons are NOT excluded by this and must not be -- they are the common case.
	if (InKey.IsGesture() || (InKey.IsAxis1D() && !InKey.IsButtonAxis()) || InKey.IsAxis2D() || InKey.IsAxis3D())
	{
		return false;
	}

	return true;
}

void UBRSettingsValue_KeyRemap::FindConflicts(const FKey& InKey, TArray<FName>& OutConflictingMappings) const
{
	OutConflictingMappings.Reset();

	if (!IsBound() || !InKey.IsValid())
	{
		return;
	}

	const UEnhancedPlayerMappableKeyProfile* Profile = UserSettings->GetActiveKeyProfile();
	if (!Profile)
	{
		return;
	}

	TArray<FName> Names;
	Profile->GetMappingNamesForKey(InKey, Names);

	for (const FName& Name : Names)
	{
		// Our own mapping in our own slot is not a conflict -- re-binding a key to where it
		// already is has to be a no-op, not an error. The same mapping in a DIFFERENT slot is
		// kept: see the header.
		if (Name == MappingName && GetCurrentKey() == InKey)
		{
			continue;
		}

		OutConflictingMappings.AddUnique(Name);
	}
}

EBRKeyRebindResult UBRSettingsValue_KeyRemap::TryAssignKey(const FKey& InKey, bool bStealFromConflicts, TArray<FName>& OutConflicts)
{
	OutConflicts.Reset();

	if (!IsBindableKey(InKey))
	{
		return EBRKeyRebindResult::Rejected;
	}

	if (!IsBound() || !IsEditable())
	{
		return EBRKeyRebindResult::Unavailable;
	}

	FindConflicts(InKey, OutConflicts);
	if (OutConflicts.Num() > 0 && !bStealFromConflicts)
	{
		return EBRKeyRebindResult::Conflict;
	}

	UEnhancedInputUserSettings* Settings = UserSettings.Get();

	// Unbind the losers first. Done before our own map so we never sit in a state where two
	// actions claim the key -- Enhanced Input would accept it and the player would get both.
	for (const FName& Conflicting : OutConflicts)
	{
		const UEnhancedPlayerMappableKeyProfile* Profile = Settings->GetActiveKeyProfile();
		const FKeyMappingRow* Row = Profile ? Profile->FindKeyMappingRow(Conflicting) : nullptr;
		if (!Row)
		{
			continue;
		}

		for (const FPlayerKeyMapping& Mapping : Row->Mappings)
		{
			if (Mapping.GetCurrentKey() != InKey)
			{
				continue;
			}

			FMapPlayerKeyArgs UnmapArgs;
			UnmapArgs.MappingName = Conflicting;
			UnmapArgs.Slot = Mapping.GetSlot();
			UnmapArgs.NewKey = InKey;
			UnmapArgs.ProfileIdString = FString();

			FGameplayTagContainer UnmapFailure;
			Settings->UnMapPlayerKey(UnmapArgs, UnmapFailure);
			break;
		}
	}

	FGameplayTagContainer FailureReason;
	Settings->MapPlayerKey(MakeArgs(InKey), FailureReason);

	// Enhanced Input reports refusal as tags rather than a bool. An empty container is the only
	// success signal there is, so it is checked rather than assumed.
	if (!FailureReason.IsEmpty())
	{
		return EBRKeyRebindResult::Rejected;
	}

	Settings->SaveSettings();
	NotifyChanged();

	return EBRKeyRebindResult::Applied;
}

EBRKeyRebindResult UBRSettingsValue_KeyRemap::ResetKeyToDefault()
{
	if (!IsBound())
	{
		return EBRKeyRebindResult::Unavailable;
	}

	const FKey Default = GetDefaultKey();
	if (!Default.IsValid())
	{
		return EBRKeyRebindResult::Unavailable;
	}

	UEnhancedInputUserSettings* Settings = UserSettings.Get();

	FGameplayTagContainer FailureReason;
	Settings->ResetAllPlayerKeysInRow(MakeArgs(Default), FailureReason);

	if (!FailureReason.IsEmpty())
	{
		return EBRKeyRebindResult::Rejected;
	}

	Settings->SaveSettings();
	NotifyChanged();

	return EBRKeyRebindResult::Applied;
}

FText UBRSettingsValue_KeyRemap::GetDisplayValue() const
{
	const FKey Key = GetCurrentKey();
	return Key.IsValid() ? Key.GetDisplayName() : BRUI::UnknownValueText();
}

bool UBRSettingsValue_KeyRemap::IsDirty() const
{
	return IsBound() && GetCurrentKey() != BaselineKey;
}

void UBRSettingsValue_KeyRemap::ResetToDefault()
{
	ResetKeyToDefault();
}

void UBRSettingsValue_KeyRemap::CaptureBaseline()
{
	BaselineKey = GetCurrentKey();
}

void UBRSettingsValue_KeyRemap::RestoreBaseline()
{
	if (!IsBound() || GetCurrentKey() == BaselineKey || !BaselineKey.IsValid())
	{
		return;
	}

	// Steals unconditionally. Cancelling out of the screen has to land the player back exactly
	// where they started, and the only thing that could be holding the baseline key is a binding
	// they made inside this same screen -- which is precisely what cancel is undoing.
	TArray<FName> IgnoredConflicts;
	TryAssignKey(BaselineKey, /* bStealFromConflicts */ true, IgnoredConflicts);
}
