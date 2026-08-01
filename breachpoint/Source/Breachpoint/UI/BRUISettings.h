// Breachpoint. Config-driven SOFT class references for every widget the UI layer pushes.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "BRUISettings.generated.h"

class UBRActivatableWidget;
class UBRKillfeedEntryWidget;
class UBRRootLayout;

/**
 * UBRUISettings -- where the UI layer's asset references live, so that no C++ file names a
 * .uasset.
 *
 * LAW 3 IS THE WHOLE POINT. Every reference below is a `TSoftClassPtr`. A hard
 * `TSubclassOf` UPROPERTY, or any constructor-time class finder, would (a) drag every front-end
 * screen into memory at module load and (b) hard-code an asset path in code, and both are
 * findings. Resolution happens at push time, in the subsystem.
 *
 * WHY THIS IS A PLAIN UObject AND NOT A UDeveloperSettings: `UDeveloperSettings` lives in the
 * `DeveloperSettings` module, which is not in Breachpoint.Build.cs - and Build.cs is not in this
 * packet's owner_path (law 5). A `UCLASS(config = Game)` UObject reads the same ini section with
 * no new module dependency; the only thing lost is the Project Settings page. Filed as a
 * contract_gap: if the crew wants the settings page, someone with Build.cs adds
 * "DeveloperSettings" and this class changes base in one line.
 *
 * Values live in Config/DefaultGame.ini under [/Script/Breachpoint.BRUISettings]. Config/ is not
 * in this packet's owner_path either, so the ini block is a handoff, not a deliverable. Until it
 * exists every soft ref is null and the subsystem logs one honest error per attempted push
 * rather than crashing.
 */
UCLASS(config = Game, defaultconfig)
class BREACHPOINT_API UBRUISettings : public UObject
{
	GENERATED_BODY()

public:
	static const UBRUISettings& Get()
	{
		return *GetDefault<UBRUISettings>();
	}

	// -----------------------------------------------------------------------
	// Widget classes (WBP subclasses of the C++ bases in this folder)
	// -----------------------------------------------------------------------

	/** The per-local-player root layout that hosts the four layer stacks. */
	UPROPERTY(config, EditDefaultsOnly, Category = "Layers")
	TSoftClassPtr<UBRRootLayout> RootLayoutClass;

	/** Pushed onto UI.Layer.Game when a match HUD is wanted. Must derive from UBRHUDLayout. */
	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> HUDLayoutClass;

	/** Pushed onto UI.Layer.Menu at front-end entry. */
	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> MainMenuScreenClass;

	/** Pushed onto UI.Layer.GameMenu on local death (killer cam + respawn timer). */
	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> DeathOverlayClass;

	/** Pushed onto UI.Layer.GameMenu at match end (K/D/A, accuracy, medals, coach line). */
	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> CarnageReportClass;

	/** One killfeed row. Pooled - see UBRHUDLayout. */
	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRKillfeedEntryWidget> KillfeedEntryClass;

	// -----------------------------------------------------------------------
	// Killfeed tuning
	// -----------------------------------------------------------------------

	/**
	 * How many rows are visible at once. This is ALSO the widget pool size: the pool is sized to
	 * the cap so a full feed never allocates. Exceeding it drops the oldest row AND logs
	 * (LogBRUI, Warning) - a silent cap is a dishonest feed.
	 *
	 * 5 covers a 4v4 team wipe plus one. Numbers-that-are-tuning normally live in
	 * Content/Data/*.csv (law 3), but a widget pool size is a UI resource budget, not gameplay
	 * tuning, and a CSV round trip for it would put a data-table dependency in the HUD's boot path.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Killfeed", meta = (ClampMin = "1", ClampMax = "16"))
	int32 KillfeedMaxVisibleEntries = 5;

	/** Seconds a row stays up. Expiry is a one-shot timer per batch, never a tick. */
	UPROPERTY(config, EditDefaultsOnly, Category = "Killfeed", meta = (ClampMin = "0.5"))
	float KillfeedEntryLifetimeSeconds = 6.0f;

	// -----------------------------------------------------------------------
	// MVVM
	// -----------------------------------------------------------------------

	/**
	 * Names under which the two ViewModels are registered in the MVVM global collection, so a WBP
	 * can bind with ContextCreationType = GlobalViewModelCollection and zero code.
	 *
	 * Change these and every WBP binding breaks silently at runtime (MVVM resolves by name), so
	 * they are config-visible but should be treated as constants.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "MVVM")
	FName CombatViewModelContextName = FName("BRCombat");

	UPROPERTY(config, EditDefaultsOnly, Category = "MVVM")
	FName MatchViewModelContextName = FName("BRMatch");
};
