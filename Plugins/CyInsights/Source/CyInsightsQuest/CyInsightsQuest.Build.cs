// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using UnrealBuildTool;
using System.IO;

public class CyInsightsQuest : ModuleRules
{
    public CyInsightsQuest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        if (Target.Version.MajorVersion < 5) CppStandard = CppStandardVersion.Cpp17;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "OpenXRHMD", "HeadMountedDisplay",
            "AugmentedReality", "TraceLog", "CyInsightsTrace", "ApplicationCore"
        });
        DynamicallyLoadedModuleNames.Add("VulkanRHI");
        PrivateIncludePathModuleNames.AddRange(new string[] { "OpenXR", "CyInsights" });
        string OpenXR = Path.Combine(EngineDirectory, "Source", "ThirdParty", "OpenXR", "include", "openxr", "openxr.h");
        string Hook = Path.Combine(EngineDirectory, "Plugins", "Runtime", "OpenXR", "Source", "OpenXRHMD", "Public", "IOpenXRExtensionPlugin.h");
        bool Metrics = File.Exists(OpenXR) && File.Exists(Hook) &&
            File.ReadAllText(OpenXR).Contains("XR_META_performance_metrics") &&
            File.ReadAllText(Hook).Contains("OnDestroySession(XrSession");
        PublicDefinitions.Add("CYINSIGHTS_QUEST_METRICS_API=" + (Metrics ? "1" : "0"));
    }
}
