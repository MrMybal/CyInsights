// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using UnrealBuildTool;
using System.IO;
public class CyQuestGpuTest : ModuleRules {
    public CyQuestGpuTest(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]{"Core", "CoreUObject", "Engine", "InputCore"});
        if (Target.Platform == UnrealTargetPlatform.Android)
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "QuestTest_UPL.xml"));
    }
}
