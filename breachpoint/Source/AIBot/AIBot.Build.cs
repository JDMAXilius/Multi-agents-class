// AIBOT — self-contained bot framework. Halo Infinite's architecture, 1:1.
// docs/AIBOT-ROADMAP.md is the plan; ARCHITECTURE.md and FAIRPLAY.md in this
// folder are the law. THE BOUNDARY: this module depends on ENGINE modules only.
// No game module here, ever — the game depends on AIBot, never the reverse.

using UnrealBuildTool;

public class AIBot : ModuleRules
{
	public AIBot(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			// AAIBBotController is an AAIController; perception lives here too.
			"AIModule",
			// Verb and ambition identities are gameplay tags. Tags only — note
			// that "GameplayAbilities" is DELIBERATELY absent: this module can
			// not name an ASC, an ability, or an attribute. GAS purity is
			// enforced by the linker, not by discipline.
			"GameplayTags",
			"GameplayTasks",
			// Pathing diagnostics against the navmesh (the host's move tasks proved
			// this dependency shape).
			"NavigationSystem",
			// UAIBAssetSettings derives UDeveloperSettings, which lives in its own
			// module. Added 26 Aug 2026 by AIB2's rung 1: without it EVERYTHING
			// compiled and the EDITOR TARGET FAILED AT LINK with ~10 undefined
			// UDeveloperSettings symbols (ctor, dtor, GetSectionName, the
			// Z_Construct_UClass thunk). The game target linked fine, because the
			// settings object is only reachable from editor code — which is exactly
			// what made it look like an authoring bug rather than a missing module.
			// The host game module's own Build.cs carries the same line for the same
			// reason. NOT naming it literally here on purpose: AIB2's boundary check
			// greps this tree case-insensitively for the host's name, and a comment
			// that mentions it fails a gate whose whole job is to be trustworthy.
			"DeveloperSettings",
			// The executor. StateTree first; a Behavior Tree executor may join
			// later behind the same IAIBExecutor seam without new dependencies
			// (BT lives in AIModule).
			"StateTreeModule",
			"GameplayStateTreeModule",
		});

		// Every .cpp includes its own header module-root-relative — the same
		// rule (and the same missing-line lesson) the host game learned the hard way.
		PublicIncludePaths.Add(ModuleDirectory);

		// EDITOR ONLY — Execution/AIBTreeAuthoring builds ST_AIBBot from C++,
		// because a StateTree graph has no scripting surface at all. Same
		// dependency block the host game's tree authoring proved against 5.8.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"AssetTools",
				"StateTreeEditorModule",
				"PropertyBindingUtils"
			});
		}
	}
}
