#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"

#include "BRGameUserSettings.generated.h"

/**
 * Broadcast after a settings apply lands. Gameplay binds here to pick up FOV / sensitivity
 * rather than the settings object reaching into a pawn -- see the class doc.
 */
DECLARE_MULTICAST_DELEGATE(FBRGameSettingsApplied);

/**
 * `UBRGameUserSettings` -- the persisted store behind the settings screen.
 *
 * ENGAGED VIA CONFIG, NOT VIA A CAST. `Config/DefaultEngine.ini` sets
 * `[/Script/Engine.Engine] GameUserSettingsClassName=/Script/Breachpoint.BRGameUserSettings`,
 * which is what makes `UGameUserSettings::GetGameUserSettings()` hand back this class engine-wide.
 * `Get()` below is a checked convenience over that, and it returns null in a commandlet or a
 * headless run where no engine settings object exists -- every caller must handle null, and the
 * spec deliberately exercises that path.
 *
 * IT DOES NOT REACH INTO GAMEPLAY. FOV and sensitivity are values a camera and a player
 * controller need, and the tempting implementation has this object walk the world to find them.
 * It does not: it broadcasts `OnGameSettingsApplied` and the interested party binds. Two reasons
 * -- a settings object that knows about pawns cannot be tested headlessly, and on a listen server
 * "the local player" is a genuinely ambiguous thing for a GameInstance-scoped object to go find.
 *
 * SENSITIVITY AND FOV ARE CLIENT-LOCAL AND ALWAYS WILL BE. Nothing here replicates, and nothing
 * here is authoritative over anything. A client that edits its own config to a 5.0 sensitivity
 * has changed how its own mouse feels and nothing else; the server does not read these values,
 * so there is no netcode surface in this file and no `_Validate` is owed (law 1).
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class BREACHPOINT_API UBRGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UBRGameUserSettings();

	/** Null in headless/commandlet contexts where the engine built no settings object. */
	static UBRGameUserSettings* Get();

	//~ Begin UGameUserSettings interface
	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;
	//~ End UGameUserSettings interface

	// -- Look ---------------------------------------------------------------------------------

	float GetMouseSensitivity() const { return MouseSensitivity; }
	void SetMouseSensitivity(float InValue);

	/** Multiplies the hip sensitivity while aiming. Separate because 1.0 is not the same feel. */
	float GetAimSensitivityMultiplier() const { return AimSensitivityMultiplier; }
	void SetAimSensitivityMultiplier(float InValue);

	bool GetInvertVerticalLook() const { return bInvertVerticalLook; }
	void SetInvertVerticalLook(bool bInValue);

	float GetFieldOfView() const { return FieldOfView; }
	void SetFieldOfView(float InValue);

	// -- Audio --------------------------------------------------------------------------------

	float GetMasterVolume() const { return MasterVolume; }
	void SetMasterVolume(float InValue);

	float GetMusicVolume() const { return MusicVolume; }
	void SetMusicVolume(float InValue);

	float GetEffectsVolume() const { return EffectsVolume; }
	void SetEffectsVolume(float InValue);

	float GetVoiceVolume() const { return VoiceVolume; }
	void SetVoiceVolume(float InValue);

	// -- Interface ----------------------------------------------------------------------------

	FName GetCrosshairStyleId() const { return CrosshairStyleId; }
	void SetCrosshairStyleId(FName InValue);

	FName GetColorBlindModeId() const { return ColorBlindModeId; }
	void SetColorBlindModeId(FName InValue);

	bool GetShowDamageNumbers() const { return bShowDamageNumbers; }
	void SetShowDamageNumbers(bool bInValue);

	/** Whether the killfeed carries Spotter flavour lines or just the plain kill. */
	bool GetShowSpotterLines() const { return bShowSpotterLines; }
	void SetShowSpotterLines(bool bInValue);

	/** Fired from `ApplyNonResolutionSettings`. Bind, do not poll. */
	FBRGameSettingsApplied OnGameSettingsApplied;

	// -- Shipped defaults. Public so the spec can assert against the same numbers the game uses,
	//    rather than restating them and drifting.

	static constexpr float DefaultMouseSensitivity = 1.0f;
	static constexpr float DefaultAimSensitivityMultiplier = 1.0f;
	static constexpr float DefaultFieldOfView = 103.0f;
	static constexpr float DefaultVolume = 0.8f;

	static constexpr float MinMouseSensitivity = 0.10f;
	static constexpr float MaxMouseSensitivity = 5.00f;
	static constexpr float MinAimSensitivityMultiplier = 0.10f;
	static constexpr float MaxAimSensitivityMultiplier = 2.00f;

	/**
	 * The FOV band. 70 is the narrowest that does not read as a scope, 120 the widest before the
	 * first-person weapon geometry starts to distort at the frame edge. Display bounds for a
	 * client-local preference, not balance -- there is no competitive claim attached to them.
	 */
	static constexpr float MinFieldOfView = 70.0f;
	static constexpr float MaxFieldOfView = 120.0f;

private:
	UPROPERTY(config)
	float MouseSensitivity = DefaultMouseSensitivity;

	UPROPERTY(config)
	float AimSensitivityMultiplier = DefaultAimSensitivityMultiplier;

	UPROPERTY(config)
	bool bInvertVerticalLook = false;

	UPROPERTY(config)
	float FieldOfView = DefaultFieldOfView;

	UPROPERTY(config)
	float MasterVolume = DefaultVolume;

	UPROPERTY(config)
	float MusicVolume = DefaultVolume;

	UPROPERTY(config)
	float EffectsVolume = DefaultVolume;

	UPROPERTY(config)
	float VoiceVolume = DefaultVolume;

	UPROPERTY(config)
	FName CrosshairStyleId = FName("Default");

	UPROPERTY(config)
	FName ColorBlindModeId = FName("None");

	UPROPERTY(config)
	bool bShowDamageNumbers = true;

	UPROPERTY(config)
	bool bShowSpotterLines = true;
};
