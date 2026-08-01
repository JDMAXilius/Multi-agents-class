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
