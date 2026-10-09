// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using UnrealBuildTool;
public class CyInsightsEditor : ModuleRules {
    public CyInsightsEditor(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "CyInsights", "ToolMenus", "Slate", "SlateCore"});
    }
}
