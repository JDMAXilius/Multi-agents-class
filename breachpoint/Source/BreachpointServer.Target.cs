// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

// Dedicated server target. Exists from day one on purpose: it is the Phase-2 insurance
// (BREACHPOINT-ARCHITECTURE.md §2) and the reason the engine is a source build — an
// installed engine ships no server-configuration binaries to link against.
public class BreachpointServerTarget : TargetRules
{
	public BreachpointServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "Breachpoint", "BreachpointNext" });
	}
}
