// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CyInsights : ModuleRules
{
	public CyInsights(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "Projects", "RHI", "TraceLog", "Sockets", "Networking" });
		bool Enabled = Target.Type != TargetType.Server &&
			(Target.Configuration == UnrealTargetConfiguration.Development || Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.DebugGame);
		PublicDefinitions.Add("CYINSIGHTS_ENABLED=" + (Enabled ? "1" : "0"));
		if (Target.Platform == UnrealTargetPlatform.Win64 && Enabled)
		{
			PrivateDependencyModuleNames.Add("CyGPUInspector");
			PrivateIncludePaths.Add(System.IO.Path.Combine(PluginDirectory, "..", "CyGPUInspector", "Source", "ThirdParty", "CyGPUInspector"));
		}
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
