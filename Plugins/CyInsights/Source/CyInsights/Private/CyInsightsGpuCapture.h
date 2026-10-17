// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "CyInsightsCapturePolicy.h"
#include "RHI.h"
#include "GpuProfilerTrace.h"
#include "CyInsightsVulkanTimestamp.h"
#include "CyInsightsVulkanPasses.h"
#if PLATFORM_ANDROID
#include "Android/AndroidPlatformMisc.h"
#endif

// Use the installed engine's GPU profiler. It owns timestamp queries, their
// delayed resolution and CPU/GPU clock calibration; no synthetic GT timestamps.
class FCyInsightsGpuCapture {
public:
    ~FCyInsightsGpuCapture(){Stop();CyInsightsVulkanPasses::Shutdown();CyInsightsVulkanTimestamp::Shutdown();}
    void Start(const cycapture::Options& Options,bool ChannelRequested) {
        Stop();Requested=ChannelRequested;Detailed=Options.gpu_passes;AppliedEventLevel=0;GroupRecordingEnabled=false;
        CompletionRequested=false;AppliedTimestampStage=-1;CompletionPath=TEXT("Not applicable");PassRecording=false;
        if(!Requested||!Supported())return;
        // AbsoluteTime queries in the legacy Vulkan profiler default to TOP_OF_PIPE.
        // Request completed work only on Quest; do not force render-pass splits,
        // flushes or synchronous query reads to manufacture subgroup durations.
        CompletionRequested=Options.quest_gpu_completion&&IsQuestVulkan();
        if(IsQuestVulkan()) {
#if defined(RHI_NEW_GPU_PROFILER) && RHI_NEW_GPU_PROFILER
            CompletionPath=TEXT("New GPU profiler owns timestamp stages; legacy Vulkan completion option not applied");
#else
            auto* Stage=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Vulkan.TimestampQueryStage"));
            {
                const uint32 Priority=Stage?(Stage->GetFlags()&ECVF_SetByMask):0;
                const bool MayChange=!Stage||(Priority<ECVF_SetByCommandline&&!(Stage->GetFlags()&ECVF_ReadOnly));
#if CYINSIGHTS_VULKAN_STAGE_BOUND
                if(CompletionRequested&&MayChange)Raise(TEXT("r.Vulkan.TimestampQueryStage"),1);
                AppliedTimestampStage=Stage?Stage->GetInt():-1;CompletionPath=TEXT("Native Vulkan CVar bound to RHI stage");
#else
                // An unbound CVar value is not evidence of the actual stage.
                AppliedTimestampStage=0;CompletionPath=TEXT("Legacy Vulkan stage remains TOP_OF_PIPE");
                const bool EnableCompletion=MayChange?CompletionRequested:(Stage&&Stage->GetInt()!=0);
                if(EnableCompletion) {
                    if(CyInsightsVulkanTimestamp::Enable()){AppliedTimestampStage=1;CompletionPath=TEXT("CyInsights Vulkan timestamp dispatch adapter");}
                    else CompletionPath=TEXT("Completion adapter unavailable or dispatch owned by another interceptor");
                }
#endif
            }
#endif
        }
        Raise(TEXT("r.GPUStatsEnabled"),1);
        Raise(TEXT("r.GPUTracingStatsEnabled"),1);
        // Level 2 keeps nested groups even under an RDG_EVENT_SCOPE_FINAL.
        // Level 3 adds each individual pass, and remains an explicit opt-in.
        // The explicit channel override may request GPU even when a stored
        // profile has gpu=false. Follow the channel that is actually recorded.
        auto Effective=Options;Effective.gpu=Requested;
        Raise(TEXT("r.RDG.Events"),Effective.GpuEventLevel());
        AppliedEventLevel=EventLevel();
        GroupRecordingEnabled=EnabledOrAbsent(TEXT("r.GPUStatsEnabled"))&&EnabledOrAbsent(TEXT("r.GPUTracingStatsEnabled"));
        PreviousDrawEvents=DrawEvents();
        if(!PreviousDrawEvents){SetDrawEvents(true);OwnDrawEvents=true;}
        if(CompletionRequested)PassRecording=CyInsightsVulkanPasses::Enable();
    }
    void Stop() {
        CyInsightsVulkanPasses::Disable();
        CyInsightsVulkanTimestamp::Disable();
        if(OwnDrawEvents&&DrawEvents())SetDrawEvents(PreviousDrawEvents);
        OwnDrawEvents=false;
        for(const auto& Entry:Owned) {
            auto* C=IConsoleManager::Get().FindConsoleVariable(*Entry.Name);
            if(C&&C->GetInt()==Entry.Applied&&(C->GetFlags()&ECVF_SetByMask)==ECVF_SetByCode) {
                C->ClearFlags(ECVF_SetByMask);
                C->Set(Entry.Previous,static_cast<EConsoleVariableFlags>(Entry.Priority));
            }
        }
        Owned.Reset();
    }
    bool IsRequested() const {return Requested;}
    bool RecordingEnabled() const {return GroupRecordingEnabled;}
    int32 RecordedEventLevel() const {return AppliedEventLevel;}
    bool IsCompletionRequested() const {return CompletionRequested;}
    int32 RecordedTimestampStage() const {return AppliedTimestampStage;}
    FString TimestampPath() const {return CompletionPath;}
    bool PassesEnabled() const {return PassRecording;}
    static bool IsQuestVulkan() {
#if PLATFORM_ANDROID
        return GDynamicRHI&&FCString::Stricmp(GDynamicRHI->GetName(),TEXT("Vulkan"))==0&&
            (FAndroidMisc::GetDeviceModel().Contains(TEXT("Quest"),ESearchCase::IgnoreCase)||
             FAndroidMisc::GetDeviceMake().Equals(TEXT("Oculus"),ESearchCase::IgnoreCase));
#else
        return false;
#endif
    }
    static bool TraceCompiled() {return (CYINSIGHTS_GPU_TRACE_COMPILED)!=0;}
    static int32 EventLevel() {auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.RDG.Events"));return C?C->GetInt():0;}
    bool Supported() const {
#if CYINSIGHTS_GPU_TRACE_COMPILED
        return GDynamicRHI&&GSupportsTimestampRenderQueries;
#else
        return false;
#endif
    }
    FString Reason() const {
#if !CYINSIGHTS_GPU_TRACE_COMPILED
        return TEXT("Native GPU tracing compiled out of this target");
#else
        if(!GDynamicRHI||!GSupportsTimestampRenderQueries)return TEXT("RHI timestamp queries unavailable; Android requires preparation before RHI initialization and supported hardware/driver");
        if(!Requested)return TEXT("GPU timestamps supported; GPU trace channel not requested in the current capture");
        if(!GroupRecordingEnabled)return TEXT("GPU group recording disabled by an explicit GPU stats/tracing console override");
        return Detailed?TEXT("Native per-render-frame GPU scopes; RDG pass names requested. Actual detail depends on the installed engine's profiler and compiled instrumentation"):
            TEXT("Native GPU groups requested each render frame (RDG level 2); hardware timestamps and delayed results resolved by Unreal. Coverage depends on compiled engine instrumentation");
#endif
    }
private:
    struct FOwned {FString Name;int32 Previous,Applied;uint32 Priority;};
    TArray<FOwned> Owned;
    bool Requested=false,Detailed=false,OwnDrawEvents=false,PreviousDrawEvents=false;
    bool GroupRecordingEnabled=false;
    bool CompletionRequested=false;
    bool PassRecording=false;
    int32 AppliedTimestampStage=-1;
    FString CompletionPath=TEXT("Not applicable");
    int32 AppliedEventLevel=0;
    static bool EnabledOrAbsent(const TCHAR* Name){auto* C=IConsoleManager::Get().FindConsoleVariable(Name);return !C||C->GetInt()!=0;}
    void Raise(const TCHAR* Name,int32 Value) {
        auto* C=IConsoleManager::Get().FindConsoleVariable(Name);
        if(!C||C->GetInt()>=Value||(C->GetFlags()&ECVF_ReadOnly))return;
        const FOwned Entry{FString(Name),C->GetInt(),Value,C->GetFlags()&ECVF_SetByMask};
        C->Set(Value,ECVF_SetByCode);
        if(C->GetInt()==Value&&(C->GetFlags()&ECVF_SetByMask)==ECVF_SetByCode)Owned.Add(Entry);
    }
    static bool DrawEvents(){
#if CYINSIGHTS_DRAW_EVENT_ACCESSORS
        return GetEmitDrawEvents();
#else
        return GEmitDrawEvents;
#endif
    }
    static void SetDrawEvents(bool Enabled){
#if CYINSIGHTS_DRAW_EVENT_ACCESSORS
        SetEmitDrawEvents(Enabled);
#else
        GEmitDrawEvents=Enabled;
#endif
    }
};
