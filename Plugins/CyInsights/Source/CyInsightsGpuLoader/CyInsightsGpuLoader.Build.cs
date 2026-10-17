// Adapted from the CyGPUInspector loader. Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using System.IO;
using UnrealBuildTool;

public class CyInsightsGpuLoader : ModuleRules
{
    public CyInsightsGpuLoader(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.Add("Core");
        PrivateDependencyModuleNames.Add("Projects");
        PrivateIncludePathModuleNames.Add("CyInsightsGpuWindows");
        string Folder = Path.Combine(PluginDirectory, "Binaries", "ThirdParty", "CyInsightsGpu", "Win64");
        foreach (string Name in new[] { "ReShade64.dll", "CyInsightsGpu.addon64" })
            if (File.Exists(Path.Combine(Folder, Name)))
                RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/CyInsightsGpu/Win64/" + Name);
        // Stage only files actually supplied; the native trace requires neither binary.
        if (Directory.Exists(Path.Combine(Folder, "LICENCES")))
            foreach (string FileName in Directory.GetFiles(Path.Combine(Folder, "LICENCES"), "*", SearchOption.AllDirectories))
                RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
        string License = Path.Combine(Folder, "LICENSE-CyInsightsCollector-MIT.txt");
        if (File.Exists(License)) RuntimeDependencies.Add(License, StagedFileType.NonUFS);
    }
}
