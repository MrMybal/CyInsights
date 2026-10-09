// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsights.h"
#include "CyInsightsRemoteControl.h"
#include "Async/Async.h"
#include "Trace/Trace.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "RHI.h"
#include "ICyInsightsQuestMetrics.h"
#include "Features/IModularFeatures.h"
#if PLATFORM_WINDOWS && CYINSIGHTS_ENABLED
#include "CyGPUInspector.h"
#include "CyGPUInspectorInProcessApi.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif
DEFINE_LOG_CATEGORY_STATIC(LogCyInsights, Log, All);
// These anchors are transmitted inside the native trace as well as saved locally.
#if COUNTERSTRACE_ENABLED
UE_TRACE_EVENT_BEGIN(CyInsights, Sync)
    UE_TRACE_EVENT_FIELD(uint64, CpuTicks)
    UE_TRACE_EVENT_FIELD(uint64, ObservedGameFrame)
    UE_TRACE_EVENT_FIELD(UE::Trace::WideString, SessionId)
UE_TRACE_EVENT_END()
#endif
namespace {
TSharedPtr<FJsonObject> Source(const FString& Id,const FString& Status,const FString& Path=FString(),const FString& Reason=FString()) {
    auto S=MakeShared<FJsonObject>();S->SetStringField(TEXT("id"),Id);S->SetStringField(TEXT("status"),Status);
    if(!Path.IsEmpty())S->SetStringField(TEXT("path"),Path);
    if(!Reason.IsEmpty())S->SetStringField(TEXT("reason"),Reason);return S;
}
}
void FCyInsightsModule::StartupModule() {
#if CYINSIGHTS_ENABLED
    if(IsRunningCommandlet()||IsRunningDedicatedServer())return;
    auto& C=IConsoleManager::Get();
    Commands.Add(C.RegisterConsoleCommand(TEXT("CyInsights.Start"),TEXT("Start a native trace and synchronization file"),FConsoleCommandDelegate::CreateLambda([this](){StartCapture();}),ECVF_Default));
    Commands.Add(C.RegisterConsoleCommand(TEXT("CyInsights.Stop"),TEXT("Stop this plugin's trace; preserve staging files"),FConsoleCommandDelegate::CreateLambda([this](){StopCapture();}),ECVF_Default));
    Commands.Add(C.RegisterConsoleCommand(TEXT("CyInsights.Gpu"),TEXT("Request 1..8 GPU frames from the Windows collector"),FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& A){CaptureGpu(A.Num()?FCString::Atoi(*A[0]):1);}),ECVF_Default));
    Commands.Add(C.RegisterConsoleCommand(TEXT("CyInsights.Capture"),TEXT("Record a timed native capture: CyInsights.Capture [seconds]"),FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& A){if(StartCapture())StopAtSeconds=FPlatformTime::Seconds()+FMath::Clamp(A.Num()?FCString::Atod(*A[0]):5.0,0.5,300.0);}),ECVF_Default));
    FrameHandle=FCoreDelegates::OnBeginFrame.AddRaw(this,&FCyInsightsModule::RecordFrame);
    RemoteControl=MakeUnique<FCyInsightsRemoteControl>(*this);
    TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this,&FCyInsightsModule::Tick),.01f);
#endif
}
void FCyInsightsModule::ShutdownModule() {
    RemoteControl.Reset();
    // The worker captures only its address/port; never unload while SendTo runs.
    if(bNetworkPending) {
        bCancelNetwork=true;if(NetworkStartTask.IsValid())NetworkStartTask.Wait();
        const double Until=FPlatformTime::Seconds()+1;
        do { FinishNetworkStart();if(bNetworkPending)FPlatformProcess::Sleep(.01f); }
        while(bNetworkPending&&FPlatformTime::Seconds()<Until);
    }
    StopCapture();
    if(bStopRequested) {
        const double Until=FPlatformTime::Seconds()+1;
        do { FPlatformProcess::Sleep(.01f);StopCapture(); }while(bStopRequested&&FPlatformTime::Seconds()<Until);
    }
    FCoreDelegates::OnBeginFrame.Remove(FrameHandle);FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    for(auto* C:Commands)IConsoleManager::Get().UnregisterConsoleObject(C);Commands.Empty();
}
bool FCyInsightsModule::StartCapture(const FString& Host, uint16 Port) {
#if CYINSIGHTS_ENABLED
    if(bCapturing||bNetworkPending||IsRunningCommandlet()||IsRunningDedicatedServer())return false;
    if(FTraceAuxiliary::IsConnected()){UE_LOG(LogCyInsights,Warning,TEXT("A native trace is already active; stop it before CyInsights.Start. No existing trace was changed."));return false;}
    SessionId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    FinalManifest.Reset();
    CreatedUtc=FDateTime::UtcNow().ToIso8601();
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CyInsights")/SessionId);
    if(!IFileManager::Get().MakeDirectory(*Directory,true))return false;
    SyncWriter.Reset(IFileManager::Get().CreateFileWriter(*(Directory/TEXT("sync.jsonl"))));if(!SyncWriter)return false;
    NetworkHost=Host;NetworkPort=Port;NetworkError.Reset();bCancelNetwork=false;bNetworkQueued=false;
    StopAtSeconds=0;bStopRequested=false;StartCycles=FPlatformTime::Cycles64();
    // Config override for channels; startup-only memory/net channels still need launch flags.
    FString Channels=TEXT("default,cpu,gpu,frame,bookmark,log,counters,task");
    FParse::Value(FCommandLine::Get(),TEXT("CyInsightsChannels="),Channels);
    Channels+=TEXT(",counters"); // Embedded session/frame anchors must follow the native stream.
#if PLATFORM_ANDROID
    // Quest metrics are native Counters events in this very trace, even when
    // the user selects a custom subset of Unreal channels.
    if (auto* Option=IConsoleManager::Get().FindConsoleVariable(TEXT("CyInsights.QuestGpuMetrics"))) {
        if (Option->GetInt()!=0) Channels+=TEXT(",counters");
    }
#endif
    FTraceAuxiliary::FOptions Options;Options.bExcludeTail=true;
    if(!Host.IsEmpty()) {
        // None configures the worker/channels. Its return value is false in UE 5.5
        // because no output was connected yet. Public SendTo accepts an explicit
        // port on all supported versions; Start(Network) does not in UE 5.5.
        FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::None,nullptr,*Channels,&Options);
        bNetworkPending=true;NetworkRequestedAt=FPlatformTime::Seconds();
        NetworkStartTask=Async(EAsyncExecution::Thread,[Host,Port](){
            return UE::Trace::SendTo(*Host,Port,UE::Trace::FSendFlags::ExcludeTail);
        });
        WriteManifest();return true;
    }
    if(!FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File,*(Directory/TEXT("unreal.utrace")),*Channels,&Options)){SyncWriter.Reset();return false;}
    bCapturing=true;
    TRACE_BOOKMARK(TEXT("CyInsights.Session %s"),*SessionId);
    if(!WriteManifest()){StopCapture();return false;}
    UE_LOG(LogCyInsights,Display,TEXT("Capture started: %s"),*Directory);return true;
#else
    return false;
#endif
}
void FCyInsightsModule::FinishNetworkStart() {
    if(!bNetworkPending)return;
    if(!bNetworkQueued) {
        if(!NetworkStartTask.IsReady())return;
        if(!NetworkStartTask.Get()) {
            NetworkError=TEXT("Cannot connect to the PC trace receiver");
            bNetworkPending=false;SyncWriter.Reset();StopAtSeconds=0;WriteManifest();return;
        }
        bNetworkQueued=true;
    }
    if(bCancelNetwork) {
        // SendTo queues a handle. Stop can refuse it until the writer consumes
        // that handle, so retry rather than leaking an unattended connection.
        if(!UE::Trace::Stop()&&FTraceAuxiliary::IsConnected())return;
        bNetworkPending=false;SyncWriter.Reset();StopAtSeconds=0;
        NetworkError=TEXT("Capture cancelled before connection completed");WriteManifest();return;
    }
    if(FTraceAuxiliary::IsConnected()) {
        bNetworkPending=false;bCapturing=true;
        TRACE_BOOKMARK(TEXT("CyInsights.Session %s"),*SessionId);
        WriteManifest();
    } else if(FPlatformTime::Seconds()-NetworkRequestedAt>10) {
        UE::Trace::Stop();bNetworkPending=false;SyncWriter.Reset();StopAtSeconds=0;
        NetworkError=TEXT("Native trace connection did not become active");WriteManifest();
    }
}
void FCyInsightsModule::StopCapture() {
    if(bNetworkPending){bCancelNetwork=true;StopAtSeconds=0;return;}
    if(!bCapturing)return;
    TRACE_BOOKMARK(TEXT("CyInsights.Stop %s"),*SessionId);
    // Final metric availability/sample counts belong to the completed capture.
    // Keep an authoritative snapshot for remote clients even if device disk I/O
    // fails; it must be taken before Stop invalidates the native trace identity.
    FinalManifest=CaptureManifest();
    if (!WriteManifest()) UE_LOG(LogCyInsights,Warning,TEXT("Cannot finalize capture manifest: %s"),*Directory);
    if(!FTraceAuxiliary::Stop()&&FTraceAuxiliary::IsConnected()){bStopRequested=true;return;}
    bStopRequested=false;bCapturing=false;StopAtSeconds=0;if(SyncWriter){SyncWriter->Close();SyncWriter.Reset();}
#if PLATFORM_ANDROID
    auto& Features=IModularFeatures::Get();
    if (Features.IsModularFeatureAvailable(ICyInsightsQuestMetrics::FeatureName()))
        Features.GetModularFeature<ICyInsightsQuestMetrics>(ICyInsightsQuestMetrics::FeatureName()).StopMetrics();
#endif
    UE_LOG(LogCyInsights,Display,TEXT("Native capture stopped: %s. Package only after the trace writer has closed."),*Directory);
}
void FCyInsightsModule::RecordFrame() {
    if(!bCapturing||!SyncWriter)return;
    const uint64 CpuTicks=FPlatformTime::Cycles64();
#if COUNTERSTRACE_ENABLED
    UE_TRACE_LOG(CyInsights,Sync,CountersChannel)
        << Sync.CpuTicks(CpuTicks) << Sync.ObservedGameFrame(GFrameCounter) << Sync.SessionId(*SessionId);
#endif
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("session_id"),SessionId);
    Row->SetStringField(TEXT("unreal_game_frame"),LexToString(GFrameCounter));
    Row->SetStringField(TEXT("cpu_ticks"),LexToString(CpuTicks));
    Row->SetStringField(TEXT("clock_id"),TEXT("unreal.cpu"));Row->SetStringField(TEXT("status"),TEXT("measured"));
#if PLATFORM_WINDOWS && CYINSIGHTS_ENABLED
    // A present counter sampled on the game thread is an approximate association.
    // It is never labelled as the GPU frame executed by this game tick.
    if(auto Addon=GetModuleHandleW(L"CyGPUInspectorRS.addon64")) {
        auto GetStatus=reinterpret_cast<PFN_CyGPUInspectorRS_GetStatus>(FPlatformProcess::GetDllExport(Addon,TEXT(CYGI_EXPORT_GET_STATUS)));
        CygiStatus Status{};Status.size=sizeof(Status);
        if(GetStatus&&GetStatus(&Status)) {Row->SetStringField(TEXT("collector_observed_frame"),LexToString(Status.frame_index));Row->SetStringField(TEXT("frame_relation"),TEXT("approximate"));Row->SetStringField(TEXT("capture_first_frame"),LexToString(Status.capture_first_frame));Row->SetNumberField(TEXT("capture_stage"),Status.capture_stage);}
    }
#endif
    FString Text;FJsonSerializer::Serialize(Row,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));Text+=TEXT("\n");
    FTCHARToUTF8 Utf8(*Text);SyncWriter->Serialize(const_cast<ANSICHAR*>(Utf8.Get()),Utf8.Length());
}
bool FCyInsightsModule::Tick(float) {
    FinishNetworkStart();
    if(bStopRequested)StopCapture();
    if(RemoteControl)RemoteControl->Tick();
    if(bCapturing&&!NetworkHost.IsEmpty()&&!FTraceAuxiliary::IsConnected()) {
        NetworkError=TEXT("Native trace stream interrupted");StopCapture();
    }
    if((bCapturing||bNetworkPending)&&StopAtSeconds>0&&FPlatformTime::Seconds()>=StopAtSeconds){
        StopCapture();if(FParse::Param(FCommandLine::Get(),TEXT("CyInsightsExitAfterCapture")))FPlatformMisc::RequestExit(false);
    }
    return true;
}
bool FCyInsightsModule::CaptureGpu(int32 Frames) {
#if PLATFORM_WINDOWS && CYINSIGHTS_ENABLED
    if(!bCapturing&&!StartCapture())return false;
    if(auto* GPU=FCyGPUInspectorModule::Get()){FText Message;auto Options=FCyGPUInspectorCaptureOptions::FromSettings();Options.FrameCount=FMath::Clamp(Frames,1,8);Options.bSaveBuffers=false;bool Ok=GPU->RequestCapture(Options,Message);UE_LOG(LogCyInsights,Display,TEXT("%s"),*Message.ToString());return Ok;}
#endif
    UE_LOG(LogCyInsights,Warning,TEXT("Detailed GPU commands unavailable on this platform/collector."));return false;
}
TSharedPtr<FJsonObject> FCyInsightsModule::CaptureManifest() const {
    auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("format"),TEXT("cycapture"));Root->SetNumberField(TEXT("version"),1);Root->SetStringField(TEXT("session_id"),SessionId);
    auto Meta=MakeShared<FJsonObject>();Meta->SetStringField(TEXT("project"),FApp::GetProjectName());Meta->SetStringField(TEXT("engine_version"),FEngineVersion::Current().ToString());
    Meta->SetStringField(TEXT("platform"),FPlatformProperties::PlatformName());Meta->SetStringField(TEXT("graphics_api"),GDynamicRHI?GDynamicRHI->GetName():TEXT("unavailable"));
    Meta->SetStringField(TEXT("gpu"),FPlatformMisc::GetPrimaryGPUBrand());Meta->SetStringField(TEXT("cpu"),FPlatformMisc::GetCPUBrand());Meta->SetStringField(TEXT("created_utc"),CreatedUtc);Root->SetObjectField(TEXT("metadata"),Meta);
    TArray<TSharedPtr<FJsonValue>> Sources;
    Sources.Add(MakeShared<FJsonValueObject>(Source(TEXT("unreal.trace"),TEXT("measured"),TEXT("unreal.utrace"))));
#if COUNTERSTRACE_ENABLED
    Sources.Add(MakeShared<FJsonValueObject>(Source(TEXT("unreal.sync"),TEXT("measured"),NetworkHost.IsEmpty()?TEXT("sync.jsonl"):TEXT("unreal.utrace"),NetworkHost.IsEmpty()?FString():TEXT("Embedded CyInsights.Sync events: CPU observation, not GPU execution frame"))));
#else
    Sources.Add(MakeShared<FJsonValueObject>(Source(TEXT("unreal.sync"),NetworkHost.IsEmpty()?TEXT("measured"):TEXT("unavailable"),NetworkHost.IsEmpty()?TEXT("sync.jsonl"):FString(),NetworkHost.IsEmpty()?FString():TEXT("Native Counters tracing disabled in this target"))));
#endif
    if(!NetworkHost.IsEmpty()) {
        auto Network=MakeShared<FJsonObject>();Network->SetStringField(TEXT("transport"),TEXT("native Unreal TCP trace"));
        Network->SetStringField(TEXT("receiver"),NetworkHost);Network->SetNumberField(TEXT("receiver_port"),NetworkPort);
        Network->SetStringField(TEXT("clock_relation"),TEXT("device CPU clock; PC receipt time is not execution time"));
        Root->SetObjectField(TEXT("network"),Network);
    }
#if PLATFORM_ANDROID
    FCyInsightsQuestMetricsStatus Status;
    Status.Reason=TEXT("Quest OpenXR collector module unavailable");
    auto& Features=IModularFeatures::Get();
    if (Features.IsModularFeatureAvailable(ICyInsightsQuestMetrics::FeatureName()))
        Status=Features.GetModularFeature<ICyInsightsQuestMetrics>(ICyInsightsQuestMetrics::FeatureName()).GetMetricsStatus();
    FGuid NativeSessionGuid,NativeTraceGuid;
    const bool Current=FTraceAuxiliary::IsConnected(NativeSessionGuid,NativeTraceGuid)&&
        Status.TraceGuid==NativeTraceGuid.ToString(EGuidFormats::DigitsWithHyphens);
    const bool Measured=Current&&Status.ValidSamples>0;
    auto Metrics=Source(TEXT("meta.openxr.metrics"),Measured?TEXT("measured"):TEXT("unavailable"),
        Measured?TEXT("unreal.utrace"):FString(),Status.Reason);
    Metrics->SetStringField(TEXT("collector"),TEXT("cyinsights.quest.openxr"));
    Metrics->SetStringField(TEXT("provider"),TEXT("Counters; CyInsightsQuest"));
    Metrics->SetStringField(TEXT("sample_clock"),TEXT("unreal.cpu"));
    Metrics->SetStringField(TEXT("timestamp_meaning"),TEXT("runtime value observed by plugin, not GPU execution time"));
    Metrics->SetStringField(TEXT("measurement_interval"),TEXT("unavailable: determined by runtime"));
    Metrics->SetStringField(TEXT("execution_frame_relation"),TEXT("unavailable"));
    Metrics->SetStringField(TEXT("valid_sample_count"),LexToString(Current?Status.ValidSamples:0));
    Metrics->SetNumberField(TEXT("available_counter_count"),Status.CounterCount);
    Sources.Add(MakeShared<FJsonValueObject>(Metrics));
#endif
    Sources.Add(MakeShared<FJsonValueObject>(Source(TEXT("gpu.commands"),TEXT("unavailable"),FString(),TEXT("Attach an exported collector capture after GPU completion"))));
    Sources.Add(MakeShared<FJsonValueObject>(Source(TEXT("gpu.cpu_calibration"),TEXT("unavailable"),FString(),TEXT("Collector has no calibrated CPU/GPU clock pairs"))));
    Root->SetArrayField(TEXT("sources"),Sources);
    auto Clock=MakeShared<FJsonObject>();Clock->SetStringField(TEXT("id"),TEXT("unreal.cpu"));Clock->SetStringField(TEXT("origin_ticks"),LexToString(StartCycles));
    Clock->SetNumberField(TEXT("frequency_hz"),1.0/FPlatformTime::GetSecondsPerCycle64());Clock->SetStringField(TEXT("timeline_origin"),TEXT("plugin.start; native trace origin requires analyzer correlation"));
    Root->SetArrayField(TEXT("clocks"),{MakeShared<FJsonValueObject>(Clock)});
    return Root;
}
bool FCyInsightsModule::WriteManifest() {
    auto Root=CaptureManifest();
    FString Text;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
IMPLEMENT_MODULE(FCyInsightsModule,CyInsights)
