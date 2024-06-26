﻿// Copyright Viktor Pramberg. All Rights Reserved.
using UnrealBuildTool;

public class NeatFunctions : ModuleRules
{
	public NeatFunctions(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"BlueprintGraph",
			"NeatFunctionsRuntime",
		});
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"SlateCore",
			"Slate",
			"UnrealEd",
			"Kismet",
			"KismetCompiler",
			"Projects",
			"GraphEditor",
		});
	}
}