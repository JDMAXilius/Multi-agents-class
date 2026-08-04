#include "UI/Settings/BRGameUserSettings.h"

#include "Engine/Engine.h"

UBRGameUserSettings::UBRGameUserSettings()
{
	// Deliberately empty of side effects. UGameUserSettings is constructed early and in contexts
	// (CDO creation, cooking) where touching engine subsystems is not safe. Defaults come from
	// the member initialisers above and from SetToDefaults.
}

UBRGameUserSettings* UBRGameUserSettings::Get()
{
	// Null in a commandlet or headless run: GEngine exists but built no settings object, or
	// GEngine itself is absent. Callers handle it -- the settings registry treats an absent
	// store as "every leaf unbound", which is why the headless spec can run at all.
	return GEngine ? Cast<UBRGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UBRGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	MouseSensitivity = DefaultMouseSensitivity;
	AimSensitivityMultiplier = DefaultAimSensitivityMultiplier;
	bInvertVerticalLook = false;
	FieldOfView = DefaultFieldOfView;

	MasterVolume = DefaultVolume;
	MusicVolume = DefaultVolume;
	EffectsVolume = DefaultVolume;
	VoiceVolume = DefaultVolume;

	CrosshairStyleId = FName("Default");
	ColorBlindModeId = FName("None");
	bShowDamageNumbers = true;
	bShowSpotterLines = true;
}

void UBRGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	// The ONE place gameplay learns that any of this moved. No walking the world for a pawn from
	// here -- see the class doc for why that stays out of this file.
	OnGameSettingsApplied.Broadcast();
}

void UBRGameUserSettings::SetMouseSensitivity(float InValue)
{
	MouseSensitivity = FMath::Clamp(InValue, MinMouseSensitivity, MaxMouseSensitivity);
}

void UBRGameUserSettings::SetAimSensitivityMultiplier(float InValue)
{
	AimSensitivityMultiplier = FMath::Clamp(InValue, MinAimSensitivityMultiplier, MaxAimSensitivityMultiplier);
}

void UBRGameUserSettings::SetInvertVerticalLook(bool bInValue)
{
	bInvertVerticalLook = bInValue;
}

void UBRGameUserSettings::SetFieldOfView(float InValue)
{
	FieldOfView = FMath::Clamp(InValue, MinFieldOfView, MaxFieldOfView);
}

void UBRGameUserSettings::SetMasterVolume(float InValue)
{
	MasterVolume = FMath::Clamp(InValue, 0.0f, 1.0f);
}

void UBRGameUserSettings::SetMusicVolume(float InValue)
{
	MusicVolume = FMath::Clamp(InValue, 0.0f, 1.0f);
}

void UBRGameUserSettings::SetEffectsVolume(float InValue)
{
	EffectsVolume = FMath::Clamp(InValue, 0.0f, 1.0f);
}

void UBRGameUserSettings::SetVoiceVolume(float InValue)
{
	VoiceVolume = FMath::Clamp(InValue, 0.0f, 1.0f);
}

void UBRGameUserSettings::SetCrosshairStyleId(FName InValue)
{
	CrosshairStyleId = InValue;
}

void UBRGameUserSettings::SetColorBlindModeId(FName InValue)
{
	ColorBlindModeId = InValue;
}

void UBRGameUserSettings::SetShowDamageNumbers(bool bInValue)
{
	bShowDamageNumbers = bInValue;
}

void UBRGameUserSettings::SetShowSpotterLines(bool bInValue)
{
	bShowSpotterLines = bInValue;
}
