// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using System.IO;
using UnrealBuildTool;

public class CyInsightsGpuWindows : ModuleRules
{
    public CyInsightsGpuWindows(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "DeveloperSettings" });
        PrivateDependencyModuleNames.AddRange(new[] { "Projects", "RenderCore", "RHI" });
        PrivateIncludePaths.Add(Path.Combine(PluginDirectory, "Source", "ThirdParty", "CyInsightsGpu"));
        bool Enabled = Target.Platform == UnrealTargetPlatform.Win64 && Target.Type != TargetType.Server &&
            (Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.DebugGame ||
             Target.Configuration == UnrealTargetConfiguration.Development);
        PublicDefinitions.Add("CYINSIGHTS_WINDOWS_GPU_ENABLED=" + (Enabled ? "1" : "0"));
    }
}
