#include "UI/Settings/BRSettingsRegistry.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/Settings/BRGameUserSettings.h"
#include "UI/Settings/BRSettingsKeyRemap.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#define LOCTEXT_NAMESPACE "BRSettings"

const FName UBRSettingsRegistry::TabGameplay(TEXT("Gameplay"));
const FName UBRSettingsRegistry::TabVideo(TEXT("Video"));
const FName UBRSettingsRegistry::TabAudio(TEXT("Audio"));
const FName UBRSettingsRegistry::TabControls(TEXT("Controls"));

namespace BRSettingsRegistryInternal
{
	static const FName OptionOn(TEXT("On"));
	static const FName OptionOff(TEXT("Off"));

	/** `0` in `UGameUserSettings` means no limit, which is a value the player picks by name. */
	static const FName OptionUnlimited(TEXT("Unlimited"));

	/** A resolution's id is its own text: "1920x1080". Stable across driver reorderings. */
	static FName MakeResolutionId(FIntPoint Resolution)
	{
		return FName(*FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y));
	}

	static FText MakeResolutionText(FIntPoint Resolution)
	{
		return FText::FromString(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y));
	}

	/** Parse "1920x1080" back. Returns false rather than guessing on anything malformed. */
	static bool ParseResolutionId(FName Id, FIntPoint& OutResolution)
	{
		FString Left;
		FString Right;
		if (!Id.ToString().Split(TEXT("x"), &Left, &Right))
		{
			return false;
		}

		if (!Left.IsNumeric() || !Right.IsNumeric())
		{
			return false;
		}

		OutResolution = FIntPoint(FCString::Atoi(*Left), FCString::Atoi(*Right));
		return OutResolution.X > 0 && OutResolution.Y > 0;
	}
}

void UBRSettingsRegistry::Build(ULocalPlayer* LocalPlayer)
{
	Tabs.Reset();
	TabsView.Reset();
	AllLeaves.Reset();
	KeyRemapLeaves.Reset();

	FrameRateLimitLeaf = nullptr;
	VSyncLeaf = nullptr;
	ResolutionLeaf = nullptr;

	// Both stores are allowed to be absent. Everything below still builds; the leaves simply
	// never bind, and `IsBound()` is how anything downstream finds out. See the class doc.
	GameSettings = UBRGameUserSettings::Get();

	if (LocalPlayer)
	{
		if (const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSettings = InputSubsystem->GetUserSettings();
		}
	}

	BuildGameplayTab();
	BuildVideoTab();
	BuildAudioTab();
	BuildControlsTab();

	RefreshEditStates();
	CaptureBaselines();
}

// ---------------------------------------------------------------------------------------------
// Tab construction
// ---------------------------------------------------------------------------------------------

void UBRSettingsRegistry::BuildGameplayTab()
{
	UBRSettingsCollection* Tab = MakeCollection(nullptr, TabGameplay,
		LOCTEXT("Tab_Gameplay", "Gameplay"), FText::GetEmpty());

	UBRSettingsCollection* Look = MakeCollection(Tab, FName("Gameplay.Look"),
		LOCTEXT("Section_Look", "Look"), FText::GetEmpty());

	TWeakObjectPtr<UBRGameUserSettings> Settings = GameSettings;

	MakeScalar(Look, FName("Look.MouseSensitivity"),
		LOCTEXT("Setting_MouseSensitivity", "Mouse Sensitivity"),
		LOCTEXT("Desc_MouseSensitivity", "How far the view turns for a given mouse movement. Applies while hip-firing."),
		UBRGameUserSettings::MinMouseSensitivity, UBRGameUserSettings::MaxMouseSensitivity,
		0.05f, EBRScalarDisplayFormat::Raw,
		[Settings] { return Settings.IsValid() ? Settings->GetMouseSensitivity() : 0.0f; },
		[Settings](float V) { if (Settings.IsValid()) { Settings->SetMouseSensitivity(V); } });

	MakeScalar(Look, FName("Look.AimSensitivity"),
		LOCTEXT("Setting_AimSensitivity", "Aim Sensitivity"),
		LOCTEXT("Desc_AimSensitivity", "Multiplies mouse sensitivity while aiming down sights."),
		UBRGameUserSettings::MinAimSensitivityMultiplier, UBRGameUserSettings::MaxAimSensitivityMultiplier,
		0.05f, EBRScalarDisplayFormat::Multiplier,
		[Settings] { return Settings.IsValid() ? Settings->GetAimSensitivityMultiplier() : 0.0f; },
		[Settings](float V) { if (Settings.IsValid()) { Settings->SetAimSensitivityMultiplier(V); } });

	MakeToggle(Look, FName("Look.InvertVertical"),
		LOCTEXT("Setting_InvertLook", "Invert Vertical Look"),
		LOCTEXT("Desc_InvertLook", "Pushing the mouse or stick forward looks down instead of up."),
		[Settings] { return Settings.IsValid() ? Settings->GetInvertVerticalLook() : false; },
		[Settings](bool V) { if (Settings.IsValid()) { Settings->SetInvertVerticalLook(V); } });

	MakeScalar(Look, FName("Look.FieldOfView"),
		LOCTEXT("Setting_FOV", "Field of View"),
		LOCTEXT("Desc_FOV", "How wide the first-person view is, in degrees."),
		UBRGameUserSettings::MinFieldOfView, UBRGameUserSettings::MaxFieldOfView,
		1.0f, EBRScalarDisplayFormat::Integer,
		[Settings] { return Settings.IsValid() ? Settings->GetFieldOfView() : 0.0f; },
		[Settings](float V) { if (Settings.IsValid()) { Settings->SetFieldOfView(V); } });

	UBRSettingsCollection* Hud = MakeCollection(Tab, FName("Gameplay.Interface"),
		LOCTEXT("Section_Interface", "Interface"), FText::GetEmpty());

	MakeToggle(Hud, FName("Interface.DamageNumbers"),
		LOCTEXT("Setting_DamageNumbers", "Damage Numbers"),
		LOCTEXT("Desc_DamageNumbers", "Show a floating number for each point of damage dealt."),
		[Settings] { return Settings.IsValid() ? Settings->GetShowDamageNumbers() : false; },
		[Settings](bool V) { if (Settings.IsValid()) { Settings->SetShowDamageNumbers(V); } });

	MakeToggle(Hud, FName("Interface.SpotterLines"),
		LOCTEXT("Setting_SpotterLines", "Announcer Lines"),
		LOCTEXT("Desc_SpotterLines", "Show the Spotter's commentary alongside killfeed entries."),
		[Settings] { return Settings.IsValid() ? Settings->GetShowSpotterLines() : false; },
		[Settings](bool V) { if (Settings.IsValid()) { Settings->SetShowSpotterLines(V); } });

	TArray<FBRSettingsOption> ColorBlindOptions;
	ColorBlindOptions.Emplace(FName("None"), LOCTEXT("ColorBlind_None", "Off"));
	ColorBlindOptions.Emplace(FName("Deuteranope"), LOCTEXT("ColorBlind_Deuteranope", "Deuteranopia"));
	ColorBlindOptions.Emplace(FName("Protanope"), LOCTEXT("ColorBlind_Protanope", "Protanopia"));
	ColorBlindOptions.Emplace(FName("Tritanope"), LOCTEXT("ColorBlind_Tritanope", "Tritanopia"));

	MakeDiscrete(Hud, FName("Interface.ColorBlindMode"),
		LOCTEXT("Setting_ColorBlind", "Colour Blind Mode"),
		LOCTEXT("Desc_ColorBlind", "Shifts the team and threat colours for common types of colour blindness."),
		MoveTemp(ColorBlindOptions),
		[Settings]() -> FName { return Settings.IsValid() ? Settings->GetColorBlindModeId() : NAME_None; },
		[Settings](FName V) { if (Settings.IsValid()) { Settings->SetColorBlindModeId(V); } });
}

void UBRSettingsRegistry::BuildVideoTab()
{
	UBRSettingsCollection* Tab = MakeCollection(nullptr, TabVideo,
		LOCTEXT("Tab_Video", "Video"), FText::GetEmpty());

	UBRSettingsCollection* Display = MakeCollection(Tab, FName("Video.Display"),
		LOCTEXT("Section_Display", "Display"), FText::GetEmpty());

	TWeakObjectPtr<UBRGameUserSettings> Settings = GameSettings;

	TArray<FBRSettingsOption> WindowModes;
	WindowModes.Emplace(FName("Fullscreen"), LOCTEXT("Window_Fullscreen", "Fullscreen"));
	WindowModes.Emplace(FName("Borderless"), LOCTEXT("Window_Borderless", "Borderless"));
	WindowModes.Emplace(FName("Windowed"), LOCTEXT("Window_Windowed", "Windowed"));

	MakeDiscrete(Display, FName("Video.WindowMode"),
		LOCTEXT("Setting_WindowMode", "Window Mode"),
		LOCTEXT("Desc_WindowMode", "Whether the game takes the whole display."),
		MoveTemp(WindowModes),
		// Explicit `-> FName`: NAME_None is an `EName`, not an `FName`, so a lambda that returns
		// it first deduces EName and every `return FName(...)` after it fails to convert. Naming
		// the return type is the fix; writing `FName()` instead of NAME_None would also compile
		// but reads as an empty name rather than the documented "no value" constant.
		[Settings]() -> FName
		{
			if (!Settings.IsValid())
			{
				return NAME_None;
			}

			switch (Settings->GetFullscreenMode())
			{
			case EWindowMode::Fullscreen:			return FName("Fullscreen");
			case EWindowMode::WindowedFullscreen:	return FName("Borderless");
			case EWindowMode::Windowed:
			default:								return FName("Windowed");
			}
		},
		[Settings](FName V)
		{
			if (!Settings.IsValid())
			{
				return;
			}

			const EWindowMode::Type Mode =
				V == FName("Fullscreen") ? EWindowMode::Fullscreen :
				V == FName("Borderless") ? EWindowMode::WindowedFullscreen :
				EWindowMode::Windowed;

			Settings->SetFullscreenMode(Mode);
		});

	ResolutionLeaf = MakeDiscrete(Display, FName("Video.Resolution"),
		LOCTEXT("Setting_Resolution", "Resolution"),
		LOCTEXT("Desc_Resolution", "The rendered size of the game window."),
		TArray<FBRSettingsOption>(),
		[Settings]() -> FName
		{
			return Settings.IsValid()
				? BRSettingsRegistryInternal::MakeResolutionId(Settings->GetScreenResolution())
				: NAME_None;
		},
		[Settings](FName V)
		{
			FIntPoint Resolution;
			if (Settings.IsValid() && BRSettingsRegistryInternal::ParseResolutionId(V, Resolution))
			{
				Settings->SetScreenResolution(Resolution);
			}
		});

	PopulateResolutionOptions();

	TArray<FBRSettingsOption> OnOff;
	OnOff.Emplace(BRSettingsRegistryInternal::OptionOn, LOCTEXT("Common_On", "On"));
	OnOff.Emplace(BRSettingsRegistryInternal::OptionOff, LOCTEXT("Common_Off", "Off"));

	VSyncLeaf = MakeDiscrete(Display, FName("Video.VSync"),
		LOCTEXT("Setting_VSync", "V-Sync"),
		LOCTEXT("Desc_VSync", "Match the display's refresh rate to remove tearing, at the cost of input latency."),
		OnOff,
		[Settings]() -> FName
		{
			return Settings.IsValid() && Settings->IsVSyncEnabled()
				? BRSettingsRegistryInternal::OptionOn
				: BRSettingsRegistryInternal::OptionOff;
		},
		[Settings](FName V)
		{
			if (Settings.IsValid())
			{
				Settings->SetVSyncEnabled(V == BRSettingsRegistryInternal::OptionOn);
			}
		});

	TArray<FBRSettingsOption> FrameRates;
	FrameRates.Emplace(FName("30"), LOCTEXT("Fps_30", "30 FPS"));
	FrameRates.Emplace(FName("60"), LOCTEXT("Fps_60", "60 FPS"));
	FrameRates.Emplace(FName("120"), LOCTEXT("Fps_120", "120 FPS"));
	FrameRates.Emplace(FName("144"), LOCTEXT("Fps_144", "144 FPS"));
	FrameRates.Emplace(FName("240"), LOCTEXT("Fps_240", "240 FPS"));
	FrameRates.Emplace(BRSettingsRegistryInternal::OptionUnlimited, LOCTEXT("Fps_Unlimited", "Unlimited"));

	FrameRateLimitLeaf = MakeDiscrete(Display, FName("Video.FrameRateLimit"),
		LOCTEXT("Setting_FrameRateLimit", "Frame Rate Limit"),
		LOCTEXT("Desc_FrameRateLimit", "Caps how many frames per second the game renders."),
		MoveTemp(FrameRates),
		// See the `-> FName` note on the window-mode getter above.
		[Settings]() -> FName
		{
			if (!Settings.IsValid())
			{
				return NAME_None;
			}

			const float Limit = Settings->GetFrameRateLimit();

			// UGameUserSettings stores "no limit" as 0. Anything at or below zero is unlimited;
			// a negative limit is not a state we can render and is not worth a second name.
			return Limit <= 0.0f
				? BRSettingsRegistryInternal::OptionUnlimited
				: FName(*FString::Printf(TEXT("%d"), FMath::RoundToInt(Limit)));
		},
		[Settings](FName V)
		{
			if (!Settings.IsValid())
			{
				return;
			}

			Settings->SetFrameRateLimit(
				V == BRSettingsRegistryInternal::OptionUnlimited ? 0.0f : FCString::Atof(*V.ToString()));
		});

	UBRSettingsCollection* Quality = MakeCollection(Tab, FName("Video.Quality"),
		LOCTEXT("Section_Quality", "Quality"), FText::GetEmpty());

	TArray<FBRSettingsOption> QualityLevels;
	QualityLevels.Emplace(FName("0"), LOCTEXT("Quality_Low", "Low"));
	QualityLevels.Emplace(FName("1"), LOCTEXT("Quality_Medium", "Medium"));
	QualityLevels.Emplace(FName("2"), LOCTEXT("Quality_High", "High"));
	QualityLevels.Emplace(FName("3"), LOCTEXT("Quality_Epic", "Epic"));

	MakeDiscrete(Quality, FName("Video.OverallQuality"),
		LOCTEXT("Setting_OverallQuality", "Overall Quality"),
		LOCTEXT("Desc_OverallQuality", "Sets every individual quality option at once."),
		MoveTemp(QualityLevels),
		// See the `-> FName` note on the window-mode getter above.
		[Settings]() -> FName
		{
			if (!Settings.IsValid())
			{
				return NAME_None;
			}

			// -1 means the individual options do not agree on a preset. Left unresolved on
			// purpose: it maps to no option, so the row reads as the em dash rather than
			// claiming a preset the machine is not actually running.
			const int32 Level = Settings->GetOverallScalabilityLevel();
			return Level < 0 ? NAME_None : FName(*FString::FromInt(Level));
		},
		[Settings](FName V)
		{
			if (Settings.IsValid())
			{
				Settings->SetOverallScalabilityLevel(FCString::Atoi(*V.ToString()));
			}
		});
}

void UBRSettingsRegistry::BuildAudioTab()
{
	UBRSettingsCollection* Tab = MakeCollection(nullptr, TabAudio,
		LOCTEXT("Tab_Audio", "Audio"), FText::GetEmpty());

	UBRSettingsCollection* Volume = MakeCollection(Tab, FName("Audio.Volume"),
		LOCTEXT("Section_Volume", "Volume"), FText::GetEmpty());

	TWeakObjectPtr<UBRGameUserSettings> Settings = GameSettings;

	MakeScalar(Volume, FName("Audio.Master"),
		LOCTEXT("Setting_MasterVolume", "Master"),
		LOCTEXT("Desc_MasterVolume", "Scales every other volume."),
		0.0f, 1.0f, 0.05f, EBRScalarDisplayFormat::Percent,
		[Settings] { return Settings.IsValid() ? Settings->GetMasterVolume() : 0.0f; },
		[Settings](float V) { if (Settings.IsValid()) { Settings->SetMasterVolume(V); } });

	MakeScalar(Volume, FName("Audio.Effects"),
		LOCTEXT("Setting_EffectsVolume", "Effects"),
		LOCTEXT("Desc_EffectsVolume", "Weapons, footsteps and the world."),
		0.0f, 1.0f, 0.05f, EBRScalarDisplayFormat::Percent,
		[Settings] { return Settings.IsValid() ? Settings->GetEffectsVolume() : 0.0f; },
		[Settings](float V) { if (Settings.IsValid()) { Settings->SetEffectsVolume(V); } });

	MakeScalar(Volume, FName("Audio.Music"),
		LOCTEXT("Setting_MusicVolume", "Music"),
		LOCTEXT("Desc_MusicVolume", "The score."),
		0.0f, 1.0f, 0.05f, EBRScalarDisplayFormat::Percent,
		[Settings] { return Settings.IsValid() ? Settings->GetMusicVolume() : 0.0f; },
		[Settings](float V) { if (Settings.IsValid()) { Settings->SetMusicVolume(V); } });

	MakeScalar(Volume, FName("Audio.Voice"),
		LOCTEXT("Setting_VoiceVolume", "Voice"),
		LOCTEXT("Desc_VoiceVolume", "Teammate voice chat and the Spotter."),
		0.0f, 1.0f, 0.05f, EBRScalarDisplayFormat::Percent,
		[Settings] { return Settings.IsValid() ? Settings->GetVoiceVolume() : 0.0f; },
		[Settings](float V) { if (Settings.IsValid()) { Settings->SetVoiceVolume(V); } });
}

void UBRSettingsRegistry::BuildControlsTab()
{
	UBRSettingsCollection* Tab = MakeCollection(nullptr, TabControls,
		LOCTEXT("Tab_Controls", "Controls"), FText::GetEmpty());

	UEnhancedInputUserSettings* Input = InputSettings.Get();
	const UEnhancedPlayerMappableKeyProfile* Profile = Input ? Input->GetActiveKeyProfile() : nullptr;

	if (!Profile)
	{
		// Headless, or Enhanced Input user settings are disabled in project settings. The tab
		// still exists and is simply empty -- a missing tab would look like a build error, and
		// this state is legitimate in a spec run.
		return;
	}

	UBRSettingsCollection* Bindings = MakeCollection(Tab, FName("Controls.Bindings"),
		LOCTEXT("Section_Bindings", "Key Bindings"), FText::GetEmpty());

	// The profile's rows are a TMap and therefore unordered. Sorted by mapping name so the
	// screen does not reshuffle itself between runs -- an unstable settings list is the kind of
	// defect that gets reported as "the menu is haunted".
	TArray<FName> MappingNames;
	Profile->GetPlayerMappingRows().GenerateKeyArray(MappingNames);
	MappingNames.Sort(FNameLexicalLess());

	for (const FName& MappingName : MappingNames)
	{
		const FKeyMappingRow* Row = Profile->FindKeyMappingRow(MappingName);
		if (!Row || !Row->HasAnyMappings())
		{
			continue;
		}

		for (const FPlayerKeyMapping& Mapping : Row->Mappings)
		{
			UBRSettingsValue_KeyRemap* Leaf = NewObject<UBRSettingsValue_KeyRemap>(this);
			Leaf->Initialize(Input, MappingName, Mapping.GetSlot());

			const FText MappingDisplay = Mapping.GetDisplayName().IsEmpty()
				? FText::FromName(MappingName)
				: Mapping.GetDisplayName();

			// The slot is part of the row's identity as well as its label -- two slots of one
			// action must not share a settings id or the screen cannot tell them apart.
			const FName LeafId(*FString::Printf(TEXT("Bind.%s.%d"), *MappingName.ToString(), static_cast<int32>(Mapping.GetSlot())));

			Leaf->SetIdentity(LeafId, MappingDisplay,
				LOCTEXT("Desc_KeyBinding", "Press a key to rebind this action. Escape cancels."));

			RegisterLeaf(Bindings, Leaf);
			KeyRemapLeaves.Add(Leaf);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Factories
// ---------------------------------------------------------------------------------------------

UBRSettingsCollection* UBRSettingsRegistry::MakeCollection(UBRSettingsCollection* Parent, FName Id, const FText& Name, const FText& Description)
{
	UBRSettingsCollection* Collection = NewObject<UBRSettingsCollection>(this);
	Collection->SetIdentity(Id, Name, Description);

	if (Parent)
	{
		Parent->AddChild(Collection);
	}
	else
	{
		Tabs.Add(Collection);
		TabsView.Add(Collection);
	}

	return Collection;
}

void UBRSettingsRegistry::RegisterLeaf(UBRSettingsCollection* Parent, UBRSettingsDataObject* Leaf)
{
	if (!Leaf)
	{
		return;
	}

	if (Parent)
	{
		Parent->AddChild(Leaf);
	}

	AllLeaves.Add(Leaf);
	Leaf->OnSettingChanged.AddUObject(this, &UBRSettingsRegistry::HandleLeafChanged);
}

UBRSettingsValue_Scalar* UBRSettingsRegistry::MakeScalar(
	UBRSettingsCollection* Parent, FName Id, const FText& Name, const FText& Description,
	float Min, float Max, float Step, EBRScalarDisplayFormat Format,
	TFunction<float()> Getter, TFunction<void(float)> Setter)
{
	UBRSettingsValue_Scalar* Leaf = NewObject<UBRSettingsValue_Scalar>(this);
	Leaf->SetIdentity(Id, Name, Description);
	Leaf->ConfigureRange(Min, Max, Step, Format);
	Leaf->BindAccessor(MoveTemp(Getter), MoveTemp(Setter));

	RegisterLeaf(Parent, Leaf);
	return Leaf;
}

UBRSettingsValue_Discrete* UBRSettingsRegistry::MakeDiscrete(
	UBRSettingsCollection* Parent, FName Id, const FText& Name, const FText& Description,
	TArray<FBRSettingsOption> Options,
	TFunction<FName()> Getter, TFunction<void(FName)> Setter)
{
	UBRSettingsValue_Discrete* Leaf = NewObject<UBRSettingsValue_Discrete>(this);
	Leaf->SetIdentity(Id, Name, Description);
	Leaf->SetOptions(MoveTemp(Options));
	Leaf->BindAccessor(MoveTemp(Getter), MoveTemp(Setter));

	RegisterLeaf(Parent, Leaf);
	return Leaf;
}

UBRSettingsValue_Discrete* UBRSettingsRegistry::MakeToggle(
	UBRSettingsCollection* Parent, FName Id, const FText& Name, const FText& Description,
	TFunction<bool()> Getter, TFunction<void(bool)> Setter)
{
	TArray<FBRSettingsOption> Options;
	Options.Emplace(BRSettingsRegistryInternal::OptionOn, LOCTEXT("Common_On", "On"));
	Options.Emplace(BRSettingsRegistryInternal::OptionOff, LOCTEXT("Common_Off", "Off"));

	return MakeDiscrete(Parent, Id, Name, Description, MoveTemp(Options),
		[Getter = MoveTemp(Getter)]
		{
			return Getter() ? BRSettingsRegistryInternal::OptionOn : BRSettingsRegistryInternal::OptionOff;
		},
		[Setter = MoveTemp(Setter)](FName V)
		{
			Setter(V == BRSettingsRegistryInternal::OptionOn);
		});
}

void UBRSettingsRegistry::PopulateResolutionOptions()
{
	if (!ResolutionLeaf)
	{
		return;
	}

	TArray<FIntPoint> Resolutions;
	if (!UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions))
	{
		// No display to ask (headless, or a platform that does not enumerate). The leaf keeps an
		// empty option list, reads as the em dash, and refuses every set -- which is correct, not
		// a failure to handle.
		return;
	}

	// Ascending by pixel count. The platform's own order is not guaranteed to be anything.
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X * A.Y < B.X * B.Y;
	});

	TArray<FBRSettingsOption> Options;
	Options.Reserve(Resolutions.Num());

	for (const FIntPoint& Resolution : Resolutions)
	{
		Options.Emplace(
			BRSettingsRegistryInternal::MakeResolutionId(Resolution),
			BRSettingsRegistryInternal::MakeResolutionText(Resolution));
	}

	ResolutionLeaf->SetOptions(MoveTemp(Options));
}

// ---------------------------------------------------------------------------------------------
// Queries and commands
// ---------------------------------------------------------------------------------------------

UBRSettingsCollection* UBRSettingsRegistry::FindTab(FName TabId) const
{
	for (const TObjectPtr<UBRSettingsCollection>& Tab : Tabs)
	{
		if (Tab && Tab->GetSettingId() == TabId)
		{
			return Tab.Get();
		}
	}

	return nullptr;
}

void UBRSettingsRegistry::GetRowsForTab(FName TabId, TArray<UBRSettingsDataObject*>& OutRows) const
{
	OutRows.Reset();

	if (const UBRSettingsCollection* Tab = FindTab(TabId))
	{
		Tab->FlattenVisible(OutRows);
	}
}

void UBRSettingsRegistry::GetKeyRemapRows(TArray<UBRSettingsValue_KeyRemap*>& OutRows) const
{
	OutRows.Reset(KeyRemapLeaves.Num());

	for (const TObjectPtr<UBRSettingsValue_KeyRemap>& Leaf : KeyRemapLeaves)
	{
		if (Leaf)
		{
			OutRows.Add(Leaf.Get());
		}
	}
}

bool UBRSettingsRegistry::HasUnsavedChanges() const
{
	for (const TObjectPtr<UBRSettingsDataObject>& Leaf : AllLeaves)
	{
		if (Leaf && Leaf->IsDirty())
		{
			return true;
		}
	}

	return false;
}

void UBRSettingsRegistry::CaptureBaselines()
{
	for (const TObjectPtr<UBRSettingsDataObject>& Leaf : AllLeaves)
	{
		if (Leaf)
		{
			Leaf->CaptureBaseline();
		}
	}
}

void UBRSettingsRegistry::ApplyAndSave()
{
	if (UBRGameUserSettings* Settings = GameSettings.Get())
	{
		// false: do not check for a command-line override. This is the player's explicit apply.
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}

	if (UEnhancedInputUserSettings* Input = InputSettings.Get())
	{
		Input->SaveSettings();
	}

	// Re-baseline AFTER the commit. Doing it before would mean a failed apply still read as
	// "no unsaved changes", which is the one lie this function must not tell.
	CaptureBaselines();
	RefreshEditStates();
}

void UBRSettingsRegistry::CancelChanges()
{
	for (const TObjectPtr<UBRSettingsDataObject>& Leaf : AllLeaves)
	{
		if (Leaf)
		{
			Leaf->RestoreBaseline();
		}
	}

	RefreshEditStates();
}

void UBRSettingsRegistry::ResetToDefaults(FName TabId)
{
	if (TabId.IsNone())
	{
		for (const TObjectPtr<UBRSettingsCollection>& Tab : Tabs)
		{
			if (Tab)
			{
				Tab->ResetToDefault();
			}
		}
	}
	else if (UBRSettingsCollection* Tab = FindTab(TabId))
	{
		Tab->ResetToDefault();
	}

	RefreshEditStates();
}

void UBRSettingsRegistry::HandleLeafChanged(UBRSettingsDataObject* Changed)
{
	RefreshEditStates();
	OnAnySettingChanged.Broadcast(Changed);
}

void UBRSettingsRegistry::RefreshEditStates()
{
	// RE-ENTRANCY GUARD, and it is load-bearing rather than defensive decoration.
	// `SetEditState` broadcasts, which lands back in `HandleLeafChanged`, which calls this. With
	// today's single rule that terminates after one extra level, because the second pass sets the
	// same state and `SetEditState` early-returns on no-change. That termination is an accident of
	// there being one rule: two rules that each disable the other's leaf would ping-pong forever.
	// The guard makes the function safe to add rules to, which is the whole reason it is a
	// function and not an inline check.
	if (bRefreshingEditStates)
	{
		return;
	}

	TGuardValue<bool> ReentryGuard(bRefreshingEditStates, true);

	if (!FrameRateLimitLeaf || !VSyncLeaf)
	{
		return;
	}

	// A frame rate cap under V-Sync is not a second cap, it is a contradiction: the display owns
	// the cadence. Greyed with a reason rather than hidden -- the player needs to see the control
	// exists and that V-Sync is what is holding it.
	const bool bVSyncOn = VSyncLeaf->GetSelectedId() == BRSettingsRegistryInternal::OptionOn;

	FrameRateLimitLeaf->SetEditState(
		bVSyncOn ? EBRSettingsEditState::DependencyUnmet : EBRSettingsEditState::Editable,
		bVSyncOn ? LOCTEXT("Dep_FrameRateVSync", "V-Sync is setting the frame rate.") : FText::GetEmpty());
}

#undef LOCTEXT_NAMESPACE
