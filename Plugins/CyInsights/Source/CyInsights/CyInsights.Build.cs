// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using System.IO;
using UnrealBuildTool;

public class CyInsights : ModuleRules
{
    public CyInsights(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "CyInsightsTrace" });
        PrivateDependencyModuleNames.AddRange(new string[] {
            "CoreUObject", "Engine", "Json", "Projects", "RHI", "RenderCore", "TraceLog", "Sockets", "Networking", "NetCore"
        });
        bool Enabled = Target.Type != TargetType.Server &&
            (Target.Configuration == UnrealTargetConfiguration.Debug ||
             Target.Configuration == UnrealTargetConfiguration.DebugGame ||
             Target.Configuration == UnrealTargetConfiguration.Development);
        PublicDefinitions.Add("CYINSIGHTS_ENABLED=" + (Enabled ? "1" : "0"));
        // Stats changed execution context between UE releases. Inspect this
        // public header in the explicitly selected engine at plugin build time.
        string TaskGraphHeader = Path.Combine(EngineDirectory, "Source", "Runtime", "Core", "Public", "Async", "TaskGraphInterfaces.h");
        bool StatsPipe = File.Exists(TaskGraphHeader) && !File.ReadAllText(TaskGraphHeader).Contains("StatsThread");
        PrivateDefinitions.Add("CYINSIGHTS_STATS_USE_PIPE=" + (StatsPipe ? "1" : "0"));
        string Globals = Path.Combine(EngineDirectory, "Source", "Runtime", "Core", "Public", "CoreGlobals.h");
        PrivateDefinitions.Add("CYINSIGHTS_DRAW_EVENT_ACCESSORS=" + (File.ReadAllText(Globals).Contains("GetEmitDrawEvents()") ? "1" : "0"));
        string GpuTrace = Path.Combine(EngineDirectory, "Source", "Runtime", "RHI", "Public", "GpuProfilerTrace.h");
        bool NewTrace = File.ReadAllText(GpuTrace).Contains("UE_TRACE_GPU_PROFILER_ENABLED");
        PrivateDefinitions.Add("CYINSIGHTS_GPU_TRACE_COMPILED=" + (NewTrace ? "UE_TRACE_GPU_PROFILER_ENABLED" : "GPUPROFILERTRACE_ENABLED"));
        string VulkanQuery = Path.Combine(EngineDirectory,"Source","Runtime","VulkanRHI","Private","VulkanQuery.cpp");
        string QuerySource = File.Exists(VulkanQuery) ? File.ReadAllText(VulkanQuery) : "";
        // UE 5.5 registers an independent CVar but reads the unchanged static
        // stage. Later versions bind that static through FAutoConsoleVariableRef.
        bool BoundStage = QuerySource.Contains("FAutoConsoleVariableRef CVarTimestampQueryStage") || QuerySource.Contains("GTimestampQueryStageCVar.GetValueOn");
        PrivateDefinitions.Add("CYINSIGHTS_VULKAN_STAGE_BOUND="+(BoundStage?"1":"0"));
        string Loader = Path.Combine(EngineDirectory,"Source","Runtime","VulkanRHI","Private","VulkanLoader.h");
        bool VulkanDispatch = Enabled && Target.Platform == UnrealTargetPlatform.Android && File.Exists(Loader) &&
            File.ReadAllText(Loader).Contains("extern VULKANRHI_API Type Func");
        PrivateDefinitions.Add("CYINSIGHTS_VULKAN_DISPATCH="+(VulkanDispatch?"1":"0"));
        string VulkanPublic=Path.Combine(EngineDirectory,"Source","Runtime","VulkanRHI","Public","IVulkanDynamicRHI.h");
        string VulkanAPI=File.Exists(VulkanPublic)?File.ReadAllText(VulkanPublic):"";
        bool VulkanPasses=VulkanDispatch&&VulkanAPI.Contains("RHIGetVkDeviceProcAddr")&&VulkanAPI.Contains("RHIGetLoadedDeviceExtensions")&&
            File.ReadAllText(Loader).Contains("vkQueueSubmit2KHR")&&File.ReadAllText(Loader).Contains("vkCmdBeginRenderPass2KHR")&&File.ReadAllText(Loader).Contains("vkCmdEndRenderPass2KHR");
        PrivateDefinitions.Add("CYINSIGHTS_VULKAN_PASSES="+(VulkanPasses?"1":"0"));
        if(VulkanDispatch){PrivateDependencyModuleNames.Add("VulkanRHI");AddEngineThirdPartyPrivateStaticDependencies(Target,"Vulkan");}
        bool WindowsCollector = Enabled && Target.Platform == UnrealTargetPlatform.Win64;
        PrivateDefinitions.Add("CYINSIGHTS_WINDOWS_COLLECTOR=" + (WindowsCollector ? "1" : "0"));
        if (WindowsCollector) {
            PrivateDependencyModuleNames.Add("CyInsightsGpuWindows");
            PrivateIncludePaths.Add(Path.Combine(PluginDirectory, "Source", "ThirdParty", "CyInsightsGpu"));
        }
    }
}
