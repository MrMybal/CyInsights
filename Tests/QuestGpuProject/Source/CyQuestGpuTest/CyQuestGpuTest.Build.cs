// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using UnrealBuildTool;
public class CyQuestGpuTest : ModuleRules {
    public CyQuestGpuTest(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]{"Core", "CoreUObject", "Engine", "InputCore"});
    }
}
