// Copyright Zollpa LLC.

using UnrealBuildTool;

public class ZoransResistance : ModuleRules
{
	public ZoransResistance(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new []
		{
			"Core",
			"CoreUObject",
			"NetCore",
			"Engine",
			"InputCore",
			"Niagara",
			"SignificanceManager",
			"GameplayAbilities",
			"GameplayTasks",
			"GameplayTags",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"PlayFab",
			"PlayFabCommon",
			"PlayFabCpp",
			"PlayFabHelper",
			"ApplicationCore",
			"Json",
			"JsonUtilities",
			"Sockets",
			"UMG"
		});
		
		PrivateDependencyModuleNames.AddRange(new []
		{
			"CoreUObject",
			"Slate",
			"SlateCore",
			"UMG",
			"EnhancedInput",
			"AIModule",
			"GameplayCameras",
			"MotionWarping",
			"MediaAssets",
			"IKRig",
            "Steamworks"
		});
	}
}