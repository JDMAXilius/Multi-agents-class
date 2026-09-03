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
			// AAIController (BNGameMode's bot book) and UAISense_Hearing (fire/blast noise) live here.
			"AIModule",
			// QA/BNAdversarialAgent projects onto the navmesh (UNavigationSystemV1).
			"NavigationSystem",
			// The AIBot track: the game depends on the module, never the reverse —
			// the adapter folder implements its interfaces.
			"AIBot",
			// FBNImpactEffectRow carries a TEnumAsByte<EPhysicalSurface>; the enum's reflection
			// constructor lives here, and the editor target links modularly.
			"PhysicsCore",
			// UBNAssetSettings derives UDeveloperSettings, which lives in its own module.
			"DeveloperSettings",
			// R7 — the HUD. This exact set is transcribed from the OLD module's Build.cs, which
			// compiled and LINKED against this engine (ROADMAP-7 §API). SlateCore is public and
			// non-negotiable: UMG's SObjectWidget pulls SWidget/EVisibility/SNullWidget from it,
			// and omitting it compiles clean and fails at LINK with 13 unresolved externals.
			// FieldNotification is NOT listed — the old module's widgets include
			// FieldNotificationId.h without it, so it arrives transitively via ModelViewViewModel;
			// deviating from the compiled reference is the risk, not the omission. Slate (the
			// module) joins PRIVATE only if a ListView/TileView ever appears — R7 ships none,
			// its killfeed and scoreboard are fixed pools by doctrine.
			"UMG",
			"SlateCore",
			"Slate",   // FSlateApplication + the font measure service (BNTabBar)
			"CommonUI",
			"CommonInput",
			"ModelViewViewModel",

			// The MEASURED button component (UBRButton and its BRButtonStyle_* set) lives in
			// the old module, and the front-end screens bind it directly so their rows are the
			// component the Figma sheet was measured against rather than an engine UButton.
			// This is UI infrastructure, not gameplay coupling: Breachpoint.Build.cs names
			// BreachpointNext nowhere, so the edge is one-way and cannot cycle. The scoreboard
			// WBP already leaned on BRRule/BRHairlineBorder at the ASSET level; this is the same
			// borrowing, made honest in the module graph because C++ now names the type.
			"Breachpoint"
		});

		// PRIVATE: QA/BNAdversarialAgent.cpp serialises its findings report through the
		// engine's Json module (Dom/JsonObject.h, TJsonWriter). Nothing public names a
		// Json type — the report writer is the one consumer.
		PrivateDependencyModuleNames.Add("Json");

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
