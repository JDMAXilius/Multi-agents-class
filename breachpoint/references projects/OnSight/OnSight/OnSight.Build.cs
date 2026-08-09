// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnSight : ModuleRules
{
	public OnSight(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput",
			"NetCore",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"UMG", 
			"GameplayAbilities",
			"PhysicsCore",
			"MotionWarping", "Niagara", "Chooser",
			"StructUtils",
			"SlateCore",
			"Slate",
			"EngineSettings",
			"AIModule",
			"CommonUI",
		});
		
		// Wwise dependencies - only add if Wwise plugin is enabled
		// To disable Wwise compilation, set "Enabled": false for "Wwise" and "WwiseSoundEngine" plugins in OnSight.uproject
		// NOTE: Disabling Wwise will cause compilation errors in OSAudioComponent and AudioManagerSubsystem that use AkAudio
		// Comment out the following lines if you disable Wwise and remove/comment AkAudio code:
		PublicDependencyModuleNames.Add("AkAudio");
		PublicDependencyModuleNames.Add("WwiseSoundEngine");

		PrivateDependencyModuleNames.AddRange(new string[] { 
			"GameplayTags",
			"GameplayTasks", 
			"AnimGraphRuntime",
			"DeveloperSettings",
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
