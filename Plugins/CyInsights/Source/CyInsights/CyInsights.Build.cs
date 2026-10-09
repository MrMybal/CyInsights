// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using System.IO;
using UnrealBuildTool;

public class CyInsights : ModuleRules
{
    public CyInsights(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CyInsightsTrace" });
        PrivateDependencyModuleNames.AddRange(new string[] {
            "CoreUObject", "Engine", "Json", "Projects", "RHI", "TraceLog", "Sockets", "Networking"
        });
        bool Enabled = Target.Type != TargetType.Server &&
            (Target.Configuration == UnrealTargetConfiguration.Debug ||
             Target.Configuration == UnrealTargetConfiguration.DebugGame ||
             Target.Configuration == UnrealTargetConfiguration.Development);
        PublicDefinitions.Add("CYINSIGHTS_ENABLED=" + (Enabled ? "1" : "0"));
        string Collector = Path.Combine(PluginDirectory, "..", "CyGPUInspector");
        bool WindowsCollector = Enabled && Target.Platform == UnrealTargetPlatform.Win64 &&
            File.Exists(Path.Combine(Collector, "Source", "CyGPUInspector", "CyGPUInspector.Build.cs"));
        PrivateDefinitions.Add("CYINSIGHTS_WINDOWS_COLLECTOR=" + (WindowsCollector ? "1" : "0"));
        if (WindowsCollector) {
            PrivateDependencyModuleNames.Add("CyGPUInspector");
            PrivateIncludePaths.Add(Path.Combine(Collector, "Source", "ThirdParty", "CyGPUInspector"));
        }
    }
}
