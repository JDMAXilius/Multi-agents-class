#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "BRUISettings.generated.h"

class UBRActivatableWidget;
class UBRKillfeedEntryWidget;
class UBRRootLayout;
class UBRSettingsRow;

UCLASS(config = Game, defaultconfig)
class BREACHPOINT_API UBRUISettings : public UObject
{
	GENERATED_BODY()

public:
	static const UBRUISettings& Get()
	{
		return *GetDefault<UBRUISettings>();
	}

	UPROPERTY(config, EditDefaultsOnly, Category = "Layers")
	TSoftClassPtr<UBRRootLayout> RootLayoutClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> HUDLayoutClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> MainMenuScreenClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> DeathOverlayClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> CarnageReportClass;

	// -- Settings (BP78). These live HERE rather than as EditDefaultsOnly members on the
	//    screens themselves, and the reason is mechanical rather than stylistic:
	//    `Tools/gen_ui/build_wbp.py` creates widgets and writes widget properties -- it has no
	//    way to author a CDO default on the generated Blueprint. A soft class declared on the
	//    screen could therefore only ever be filled in by hand in the editor, which is the
	//    binary-asset edit law 7 and R37 exist to prevent. As config properties they are set in
	//    `Config/DefaultGame.ini`, which is text, diffable, and law 7's stated preference.

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> SettingsScreenClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> KeyRemapScreenClass;

	/**
	 * The confirm modal both settings surfaces raise -- "discard your changes?" on back-out and
	 * "that key is already bound, steal it?" on a rebind conflict. ONE class for both: it takes
	 * an `FBRConfirmRequest` payload, so the two questions differ only in their text.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> ConfirmModalClass;

	/**
	 * The DEFAULT row widget `UBRScreen_Settings::RebuildRows` instances — headers, key bindings,
	 * and anything the three typed classes below do not claim. A `BP_`-free generated WBP.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRSettingsRow> SettingsRowClass;

	/**
	 * The typed rows, one per `EBRMenuRowType` a setting can resolve to.
	 *
	 * THREE CLASSES RATHER THAN ONE, because the per-type body lives in the ASSET. `UBRMenuRow`
	 * selects a body through a `TypeSwitcher` bind, and `WBP_SettingsRow` has no switcher and no
	 * bodies — so before these existed, `RefreshFromSetting` resolved Slider / Checkbox / DropDown
	 * correctly and then rendered all three as a plain label-and-value row. A volume slider and a
	 * resolution dropdown were pixel-identical.
	 *
	 * Each is UNSET-SAFE: `RebuildRows` falls back to `SettingsRowClass` for any that is null or
	 * fails to load, so a missing entry degrades to today's behaviour instead of an empty list.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRSettingsRow> SettingsRowSliderClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRSettingsRow> SettingsRowCheckboxClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRSettingsRow> SettingsRowDropDownClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRKillfeedEntryWidget> KillfeedEntryClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Killfeed", meta = (ClampMin = "1", ClampMax = "16"))
	int32 KillfeedMaxVisibleEntries = 5;

	UPROPERTY(config, EditDefaultsOnly, Category = "Killfeed", meta = (ClampMin = "0.5"))
	float KillfeedEntryLifetimeSeconds = 6.0f;

	UPROPERTY(config, EditDefaultsOnly, Category = "MVVM")
	FName CombatViewModelContextName = FName("BRCombat");

	UPROPERTY(config, EditDefaultsOnly, Category = "MVVM")
	FName MatchViewModelContextName = FName("BRMatch");

	UPROPERTY(config, EditDefaultsOnly, Category = "MVVM")
	FName FrontEndViewModelContextName = FName("BRFrontEnd");

	UPROPERTY(config, EditDefaultsOnly, Category = "MVVM")
	FName PlayerViewModelContextName = FName("BRPlayer");
};
