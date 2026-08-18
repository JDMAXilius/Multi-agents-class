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
			"NetCore",
			// ABNBotController: AAIController + perception live in AIModule; the StateTree pair
			// carries UStateTreeAIComponent (GameplayStateTree) atop the runtime (StateTreeModule).
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			// FBNImpactEffectRow carries a TEnumAsByte<EPhysicalSurface>; the enum's reflection
			// constructor lives here, and the editor target links modularly.
			"PhysicsCore",
			// UBNAssetSettings derives UDeveloperSettings, which lives in its own module.
			"DeveloperSettings"
		});

		// PRIVATE: AbilitySystem/BNGameplayCues.cpp spawns cue FX through
		// UNiagaraFunctionLibrary, but its header types the assets as TSoftObjectPtr<UFXSystemAsset>
		// — the Engine-module base of both FX systems — so no public header names a Niagara type.
		PrivateDependencyModuleNames.Add("Niagara");

		// Every .cpp in this module includes its own header module-root-relative
		// ("Match/BNPlayerController.h"). The module has no Public/Private split, so
		// without this the root is not on the include path and NOTHING here compiles.
		// Same line the old module carries; it was missing since the module was created.
		PublicIncludePaths.Add("BreachpointNext");
	}
}
