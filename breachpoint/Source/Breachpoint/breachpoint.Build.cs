// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Breachpoint : ModuleRules
{
	public Breachpoint(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// ARCHITECTURE.md §3 dependency list. "Slate" is template-inherited and kept because
		// the surviving Variant_* sources need it.
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
			"Slate",
			// SlateCore added 1 Aug 2026: UMG's SObjectWidget pulls SWidget, EVisibility,
			// SNullWidget and FSlateAttributeMetaData, all of which live in SlateCore, not
			// Slate. Without it the module compiles clean and fails at LINK with 13
			// unresolved externals -- the failure mode is invisible until the linker runs.
			"SlateCore",
			"CommonUI",
			"CommonInput",
			"ModelViewViewModel"
		});

		// OnlineSubsystem is the API surface; the Steam *implementation* is selected by
		// the enabled OnlineSubsystemSteam plugin + DefaultEngine.ini, never linked directly.
		PrivateDependencyModuleNames.AddRange(new string[] {
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"HTTP",
			"Json"
		});

		PublicIncludePaths.AddRange(new string[] {
			"Breachpoint",
			"Breachpoint/Variant_Horror",
			"Breachpoint/Variant_Horror/UI",
			"Breachpoint/Variant_Shooter",
			"Breachpoint/Variant_Shooter/AI",
			"Breachpoint/Variant_Shooter/UI",
			"Breachpoint/Variant_Shooter/Weapons"
		});
	}
}
