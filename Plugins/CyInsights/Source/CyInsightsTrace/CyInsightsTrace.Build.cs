// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
using System.IO;
using UnrealBuildTool;

public class CyInsightsTrace : ModuleRules
{
    public CyInsightsTrace(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "TraceLog" });
        PrivateDependencyModuleNames.Add("Sockets");
        if (Target.Version.MajorVersion < 5) CppStandard = CppStandardVersion.Cpp17;
        string Runtime = Path.Combine(EngineDirectory, "Source", "Runtime");
        string Aux = File.ReadAllText(Path.Combine(Runtime, "Core", "Public", "ProfilingDebugging", "TraceAuxiliary.h"));
        string Trace = File.ReadAllText(Path.Combine(Runtime, "TraceLog", "Public", "Trace", "Trace.h"));
        PublicDefinitions.Add("CYINSIGHTS_TRACE_HAS_AUX_CAPTURE=" + (Aux.Contains("EConnectionType") ? "1" : "0"));
        PublicDefinitions.Add("CYINSIGHTS_TRACE_HAS_GUID=" + (Aux.Contains("OutTraceGuid") ? "1" : "0"));
        PublicDefinitions.Add("CYINSIGHTS_TRACE_HAS_EXCLUDE_TAIL=" + (Aux.Contains("bExcludeTail") ? "1" : "0"));
        PublicDefinitions.Add("CYINSIGHTS_TRACE_HAS_SEND_FLAGS=" + (Trace.Contains("FSendFlags") && Trace.Contains("ExcludeTail") ? "1" : "0"));
    }
}
