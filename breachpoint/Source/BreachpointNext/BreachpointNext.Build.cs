using UnrealBuildTool;

public class BreachpointNext : ModuleRules
{
	public BreachpointNext(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"NetCore"
		});

		// Every .cpp in this module includes its own header module-root-relative
		// ("Match/BNPlayerController.h"). The module has no Public/Private split, so
		// without this the root is not on the include path and NOTHING here compiles.
		// Same line the old module carries; it was missing since the module was created.
		PublicIncludePaths.Add("BreachpointNext");
	}
}
