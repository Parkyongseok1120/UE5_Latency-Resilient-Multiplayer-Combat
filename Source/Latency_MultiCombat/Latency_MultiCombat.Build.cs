// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Latency_MultiCombat : ModuleRules
{
	public Latency_MultiCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
		PrivateDependencyModuleNames.AddRange(new string[] { "RHI", "AnimationCore", "SlateCore", "EngineCameras" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AnimGraph", "AnimGraphRuntime", "BlueprintGraph" });
			PrivateDependencyModuleNames.AddRange(new string[] { "MeshDescription", "SkeletalMeshDescription", "StaticMeshDescription" });
		}
		RuntimeDependencies.Add("$(ProjectDir)/Licenses/NanumSquareNeo-OFL.txt",
			Path.GetFullPath(Path.Combine(ModuleDirectory, "../../SourceAssets/Fonts/NanumSquareNeo/OFL.txt")),
			StagedFileType.NonUFS);

	}
}
