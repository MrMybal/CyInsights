// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using UnrealBuildTool;
public class CyQuestGpuTestTarget : TargetRules {
    public CyQuestGpuTestTarget(TargetInfo Target) : base(Target) {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        ExtraModuleNames.Add("CyQuestGpuTest");
    }
}
