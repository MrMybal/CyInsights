// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class CyInsightsQuest : ModuleRules
{
    public CyInsightsQuest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "OpenXRHMD", "HeadMountedDisplay",
            "AugmentedReality", "TraceLog"
        });
        PrivateIncludePathModuleNames.AddRange(new string[] { "OpenXR", "CyInsights" });
    }
}
