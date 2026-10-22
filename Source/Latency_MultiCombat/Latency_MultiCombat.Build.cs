// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Latency_MultiCombat : ModuleRules
{
	public Latency_MultiCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
		PrivateDependencyModuleNames.Add("RHI");

	}
}
