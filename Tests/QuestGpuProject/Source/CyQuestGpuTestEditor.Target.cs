// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using UnrealBuildTool;
public class CyQuestGpuTestEditorTarget : TargetRules {
    public CyQuestGpuTestEditorTarget(TargetInfo Target) : base(Target) {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        ExtraModuleNames.Add("CyQuestGpuTest");
    }
}
