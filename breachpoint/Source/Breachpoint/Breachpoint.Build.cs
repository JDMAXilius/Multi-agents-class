// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Breachpoint : ModuleRules
{
	public Breachpoint(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// ARCHITECTURE.md §3 dependency list.
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"NavigationSystem",
			"UMG",
			// SlateCore added 1 Aug 2026: UMG's SObjectWidget pulls SWidget, EVisibility,
			// SNullWidget and FSlateAttributeMetaData, all of which live in SlateCore, not
			// Slate. Without it the module compiles clean and fails at LINK with 13
			// unresolved externals -- the failure mode is invisible until the linker runs.
			"SlateCore",
			"CommonUI",
			"CommonInput",
			"ModelViewViewModel",
			// DeveloperSettings added 3 Aug 2026 (BP78): UI/Loading/BRLoadingScreenSettings.h
			// derives UDeveloperSettings so the loading screen's knobs appear under Project
			// Settings > Game rather than only in an ini nobody opens. PUBLIC because that
			// header is included by the subsystem's own public header's consumers.
			"DeveloperSettings"
		});

		// OnlineSubsystem is the API surface; the Steam *implementation* is selected by
		// the enabled OnlineSubsystemSteam plugin + DefaultEngine.ini, never linked directly.
		PrivateDependencyModuleNames.AddRange(new string[] {
			// Slate: removed 14 Aug 2026 by BP90 on the premise that it was inherited from the
			// deleted variant templates. That premise was WRONG and the removal failed at LINK,
			// not at compile. UI/Components/BRItemGrid.cpp includes Components/TileView.h and
			// UI/Components/BRItemTile.cpp includes Components/ListView.h; those UMG headers
			// instantiate SListView<UObject*> INTO THIS MODULE'S translation unit, and its base
			// STableViewBase lives in Slate, not SlateCore. Restored 14 Aug 2026. Do not remove
			// while any BR widget owns a UListView/UTileView/UTreeView.
			// PRIVATE, not Public: no BR public header names a Slate type -- BRItemGrid.h
			// forward-declares UTileView only, so the instantiation never escapes these .cpps.
			"Slate",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"HTTP",
			"Json",
			// Niagara added 1 Aug 2026: AbilitySystem/Cues/BRGameplayCues.cpp spawns cue FX with
			// UNiagaraFunctionLibrary::SpawnSystemAtLocation. Its soft refs are typed
			// TSoftObjectPtr<UFXSystemAsset> -- the Engine-module base of BOTH Cascade and Niagara --
			// so the module compiled and linked clean without this and simply REFUSED to play every
			// Niagara system that reached the handler, naming it in a one-shot Warning. The failure
			// mode is invisible until an FX asset exists, which is later than anyone will look.
			// PRIVATE, not Public: no public header names a Niagara type (BRGameplayCues.h
			// forward-declares only UFXSystemAsset), so no dependent module needs the include path.
			"Niagara",
			// MoviePlayer added 3 Aug 2026 (BP78): UI/Loading/BRLoadingScreenSubsystem.cpp calls
			// GetMoviePlayer()->SetupLoadingScreen. It is the ONLY thing that renders while the
			// game thread is blocked loading a map -- a UMG widget on the viewport draws nothing
			// during that window, which is the window a loading screen exists to cover.
			// PRIVATE: no public header names an IGameMoviePlayer type.
			"MoviePlayer"
		});

		// The six variant-template entries were removed 14 Aug 2026 (BP90) with the
		// template sources. The module-root entry stays: every BR source includes module-
		// root-relative ("Core/BRCore.h" etc.); removing it fails compile on file-not-found.
		PublicIncludePaths.AddRange(new string[] {
			"Breachpoint"
		});
	}
}
