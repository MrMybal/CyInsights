// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsights.h"
#include "CyInsightsRemoteControl.h"
#include "CyInsightsStatsCapture.h"
#include "CyInsightsGpuCapture.h"
#include "CyInsightsSettings.h"
#include "CyInsightsCaptureJson.h"
#include "Net/Core/Trace/NetTrace.h"
#include "Async/Async.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"
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
#include "Policies/CondensedJsonPrintPolicy.h"
#include "RHI.h"
#include "ICyInsightsQuestMetrics.h"
#include "Features/IModularFeatures.h"
#if PLATFORM_ANDROID
#include <cstdio>
#endif
#if CYINSIGHTS_WINDOWS_COLLECTOR
#include "CyInsightsGpuWindows.h"
#include "CyInsightsGpuApi.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif
DEFINE_LOG_CATEGORY_STATIC(LogCyInsights, Log, All);
FCyInsightsModule::FCyInsightsModule()=default;
FCyInsightsModule::~FCyInsightsModule()=default;
// These anchors are transmitted inside the native trace as well as saved locally.
#if COUNTERSTRACE_ENABLED
// Own channel: UE 4.27's CountersChannel is not exported by Core.
UE_TRACE_CHANNEL_DEFINE(CyInsightsSyncChannel);
UE_TRACE_EVENT_BEGIN(CyInsights, Sync)
    UE_TRACE_EVENT_FIELD(uint64, CpuTicks)
    UE_TRACE_EVENT_FIELD(uint64, ObservedGameFrame)
    UE_TRACE_EVENT_FIELD(CyInsightsNativeTrace::WideString, SessionId)
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
    Commands.Add(C.RegisterConsoleVariable(TEXT("CyInsights.NetworkTraceLevel"),1,TEXT("Native NetTrace verbosity during capture (0 disables automatic activation); compile-time availability still applies"),ECVF_Default));
    Commands.Add(C.RegisterConsoleCommand(TEXT("CyInsights.Start"),TEXT("Start a native trace and synchronization file"),FConsoleCommandDelegate::CreateLambda([this](){StartCapture();}),ECVF_Default));
    Commands.Add(C.RegisterConsoleCommand(TEXT("CyInsights.Stop"),TEXT("Stop this plugin's trace; preserve staging files"),FConsoleCommandDelegate::CreateLambda([this](){StopCapture();}),ECVF_Default));
    Commands.Add(C.RegisterConsoleCommand(TEXT("CyInsights.Gpu"),TEXT("Request 1..8 GPU frames from the Windows collector"),FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& A){CaptureGpu(A.Num()?FCString::Atoi(*A[0]):1);}),ECVF_Default));
    Commands.Add(C.RegisterConsoleCommand(TEXT("CyInsights.Capture"),TEXT("Record a timed native capture: CyInsights.Capture [seconds]"),FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& A){if(StartCapture())StopAtSeconds=FPlatformTime::Seconds()+FMath::Clamp(A.Num()?FCString::Atod(*A[0]):5.0,0.5,300.0);}),ECVF_Default));
    FrameHandle=FCoreDelegates::OnBeginFrame.AddRaw(this,&FCyInsightsModule::RecordFrame);
    StatsCapture=MakeUnique<FCyInsightsStatsCapture>();
    GpuCapture=MakeUnique<FCyInsightsGpuCapture>();
    RemoteControl=MakeUnique<FCyInsightsRemoteControl>(*this);
    TickHandle=FCyInsightsTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this,&FCyInsightsModule::Tick),.01f);
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
    FCoreDelegates::OnBeginFrame.Remove(FrameHandle);FCyInsightsTicker::GetCoreTicker().RemoveTicker(TickHandle);
    StatsCapture.Reset();GpuCapture.Reset();
    for(auto* C:Commands)IConsoleManager::Get().UnregisterConsoleObject(C);Commands.Empty();
}
bool FCyInsightsModule::StartCapture(const FString& Host, uint16 Port,const cycapture::Options* Override) {
#if CYINSIGHTS_ENABLED
    if(bCapturing||bNetworkPending||IsRunningCommandlet()||IsRunningDedicatedServer())return false;
    if(CyInsightsTrace::IsConnected()){UE_LOG(LogCyInsights,Warning,TEXT("A native trace is already active; stop it before CyInsights.Start. No existing trace was changed."));return false;}
    const auto Requested=Override?*Override:UCyInsightsSettings::CaptureDefaults();
    const auto Invalid=Requested.Validate();if(!Invalid.empty()){UE_LOG(LogCyInsights,Warning,TEXT("%s"),UTF8_TO_TCHAR(Invalid.c_str()));return false;}
    CaptureOptions=Requested;GpuCaptureMessage.Reset();
    SessionId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    FinalManifest.Reset();
    CreatedUtc=FDateTime::UtcNow().ToIso8601();
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CyInsights")/SessionId);
    if(!IFileManager::Get().MakeDirectory(*Directory,true))return false;
    SyncWriter.Reset(IFileManager::Get().CreateFileWriter(*(Directory/TEXT("sync.jsonl"))));if(!SyncWriter)return false;
    NetworkHost=Host;NetworkPort=Port;NetworkError.Reset();bCancelNetwork=false;bNetworkQueued=false;
    StopAtSeconds=0;bStopRequested=false;StartCycles=FPlatformTime::Cycles64();
    // Config override for channels; startup-only memory/net channels still need launch flags.
    FString Channels=TEXT("frame,bookmark,counters,cyinsightssync");
    if(CaptureOptions.cpu)Channels+=TEXT(",cpu");if(CaptureOptions.gpu)Channels+=TEXT(",gpu");
    if(CaptureOptions.tasks)Channels+=TEXT(",task");if(CaptureOptions.network)Channels+=TEXT(",net");
    if(CaptureOptions.file_io)Channels+=TEXT(",file");if(CaptureOptions.loadtime)Channels+=TEXT(",loadtime");
    if(CaptureOptions.logs)Channels+=TEXT(",log");if(CaptureOptions.memtags)Channels+=TEXT(",memtag");
    if(!Override)FParse::Value(FCommandLine::Get(),TEXT("CyInsightsChannels="),Channels);
    Channels+=TEXT(",counters"); // Embedded session/frame anchors must follow the native stream.
    Channels+=TEXT(",cyinsightssync");
    if(CaptureOptions.frame_stats)Channels+=TEXT(",cyinsightsstats");
#if CYINSIGHTS_VULKAN_PASSES
    TArray<FString> GpuChannels;Channels.ParseIntoArray(GpuChannels,TEXT(","),true);
    if(CaptureOptions.quest_gpu_completion&&GpuChannels.ContainsByPredicate([](const FString& C){return C.Equals(TEXT("gpu"),ESearchCase::IgnoreCase)||C.Equals(TEXT("gpuchannel"),ESearchCase::IgnoreCase);}))Channels+=TEXT(",cyinsightsvulkan");
#endif
    if(CaptureOptions.frame_stats&&CaptureOptions.stat_groups_enabled)Channels+=TEXT(",stats");
    CaptureChannels=Channels;
    ConfigureQuestCapture(true);
#if PLATFORM_ANDROID
    // Quest metrics are native Counters events in this very trace, even when
    // the user selects a custom subset of Unreal channels.
    if (auto* Option=IConsoleManager::Get().FindConsoleVariable(TEXT("CyInsights.QuestGpuMetrics"))) {
        if (Option->GetInt()!=0) Channels+=TEXT(",counters");
    }
#endif
    if(!Host.IsEmpty()) {
        // None configures the worker/channels. Its return value is false in UE 5.5
        // because no output was connected yet. Public SendTo accepts an explicit
        // port on all supported versions; Start(Network) does not in UE 5.5.
        CyInsightsTrace::ConfigureChannels(Channels);
        bNetworkPending=true;NetworkRequestedAt=FPlatformTime::Seconds();
        NetworkStartTask=Async(EAsyncExecution::Thread,[Host,Port](){
            return CyInsightsTrace::SendTo(Host,Port);
        });
        WriteManifest();return true;
    }
    if(!CyInsightsTrace::StartFile(Directory/TEXT("unreal.utrace"),Channels)){
        ConfigureQuestCapture(false);
        NetworkError=CyInsightsTrace::GetError();
        UE_LOG(LogCyInsights,Error,TEXT("Cannot start native trace: %s"),*NetworkError);
        SyncWriter.Reset();WriteManifest();return false;
    }
    bCapturing=true;BeginCaptureCollectors();
    TRACE_BOOKMARK(TEXT("CyInsights.Session %s"),*SessionId);
    if(!WriteManifest()){StopCapture();return false;}
    UE_LOG(LogCyInsights,Display,TEXT("Capture started: %s"),*Directory);return true;
#else
    return false;
#endif
}
void FCyInsightsModule::ConfigureQuestCapture(bool Apply) {
#if PLATFORM_ANDROID
    auto& Features=IModularFeatures::Get();
    if(Features.IsModularFeatureAvailable(ICyInsightsQuestMetrics::FeatureName())){
        auto& Quest=Features.GetModularFeature<ICyInsightsQuestMetrics>(ICyInsightsQuestMetrics::FeatureName());
        if(Apply)Quest.ConfigureCapture(CaptureOptions.quest_metrics,CaptureOptions.quest_hz);
        else Quest.ClearCaptureConfiguration();
    }
#endif
}
void FCyInsightsModule::BeginCaptureCollectors() {
    TArray<FString> Channels;CaptureChannels.ParseIntoArray(Channels,TEXT(","),true);
    const bool GpuRequested=Channels.ContainsByPredicate([](const FString& C){return C.Equals(TEXT("gpu"),ESearchCase::IgnoreCase)||C.Equals(TEXT("gpuchannel"),ESearchCase::IgnoreCase);});
    if(GpuCapture)GpuCapture->Start(CaptureOptions,GpuRequested);
    if(GpuRequested&&GpuCapture) {
        const FString Reason=GpuCapture->Reason()+FString::Printf(TEXT("; timestamp stage=%d; %s; Vulkan passes: %s"),GpuCapture->RecordedTimestampStage(),*GpuCapture->TimestampPath(),*CyInsightsVulkanPasses::Status());
        TRACE_BOOKMARK(TEXT("CyInsights.GpuGroups %s"),*Reason);
        if(GpuCapture->Supported()&&GpuCapture->RecordingEnabled()){UE_LOG(LogCyInsights,Display,TEXT("%s"),*Reason);}
        else {UE_LOG(LogCyInsights,Warning,TEXT("%s"),*Reason);}
    }
    EnableCaptureNetwork();if(StatsCapture)StatsCapture->Start(CaptureOptions);
    if(CaptureOptions.gpu_details)CaptureGpu(CaptureOptions);
}
void FCyInsightsModule::EnableCaptureNetwork() {
    CaptureNetworkTraceLevel=0;OwnedNetworkTraceLevel=0;
#if UE_NET_TRACE_ENABLED
    TArray<FString> Names;CaptureChannels.ParseIntoArray(Names,TEXT(","),true);
    const bool Requested=Names.ContainsByPredicate([](const FString& Name){return Name.Equals(TEXT("net"),ESearchCase::IgnoreCase)||Name.Equals(TEXT("netchannel"),ESearchCase::IgnoreCase);});
    if(!Requested)return;
    auto* Option=IConsoleManager::Get().FindConsoleVariable(TEXT("CyInsights.NetworkTraceLevel"));
    const uint32 Level=Option?static_cast<uint32>(FMath::Clamp(Option->GetInt(),0,4)):1;
    if(Level&&!FNetTrace::GetTraceVerbosity()) {
        // Enable after the output is attached, so the native Init event belongs
        // to this capture, including an IP-triggered capture after game launch.
        FNetTrace::SetTraceVerbosity(Level);OwnedNetworkTraceLevel=FNetTrace::GetTraceVerbosity();
    }
    CaptureNetworkTraceLevel=FNetTrace::GetTraceVerbosity();
#endif
}
void FCyInsightsModule::RestoreCaptureNetwork() {
#if UE_NET_TRACE_ENABLED
    // Preserve a verbosity the user enabled before capture or changed during it.
    if(OwnedNetworkTraceLevel&&FNetTrace::GetTraceVerbosity()==OwnedNetworkTraceLevel)FNetTrace::SetTraceVerbosity(0);
#endif
    OwnedNetworkTraceLevel=0;
}
void FCyInsightsModule::FinishNetworkStart() {
    if(!bNetworkPending)return;
    if(!bNetworkQueued) {
        if(!NetworkStartTask.IsReady())return;
        if(!NetworkStartTask.Get()) {
            CyInsightsTrace::RestoreChannels();
            NetworkError=TEXT("Cannot connect to the PC trace receiver");
            if (!CyInsightsTrace::GetError().IsEmpty()) NetworkError=CyInsightsTrace::GetError();
            bNetworkPending=false;SyncWriter.Reset();StopAtSeconds=0;ConfigureQuestCapture(false);WriteManifest();return;
        }
        bNetworkQueued=true;
    }
    if(bCancelNetwork) {
        // SendTo queues a handle. Stop can refuse it until the writer consumes
        // that handle, so retry rather than leaking an unattended connection.
        if(!CyInsightsTrace::Stop())return;
        bNetworkPending=false;SyncWriter.Reset();StopAtSeconds=0;
        NetworkError=TEXT("Capture cancelled before connection completed");ConfigureQuestCapture(false);WriteManifest();return;
    }
    if(CyInsightsTrace::IsConnected()) {
        bNetworkPending=false;bCapturing=true;BeginCaptureCollectors();
        TRACE_BOOKMARK(TEXT("CyInsights.Session %s"),*SessionId);
        WriteManifest();
    } else if(FPlatformTime::Seconds()-NetworkRequestedAt>10) {
        CyInsightsTrace::Stop();bNetworkPending=false;SyncWriter.Reset();StopAtSeconds=0;
        NetworkError=TEXT("Native trace connection did not become active");ConfigureQuestCapture(false);WriteManifest();
    }
}
void FCyInsightsModule::StopCapture() {
    if(bNetworkPending){bCancelNetwork=true;StopAtSeconds=0;return;}
    if(!bCapturing)return;
    if(StatsCapture)StatsCapture->Stop();
    // Drain only already-completed supplemental queries while the writer is
    // still connected. Remaining in-flight passes are explicitly unavailable.
    CyInsightsVulkanPasses::Disable();
    TRACE_BOOKMARK(TEXT("CyInsights.Stop %s"),*SessionId);
    // Final metric availability/sample counts belong to the completed capture.
    // Keep an authoritative snapshot for remote clients even if device disk I/O
    // fails; it must be taken before Stop invalidates the native trace identity.
    FinalManifest=CaptureManifest();
    if (!WriteManifest()) UE_LOG(LogCyInsights,Warning,TEXT("Cannot finalize capture manifest: %s"),*Directory);
    if(!CyInsightsTrace::Stop()){bStopRequested=true;return;}
    if (!CyInsightsTrace::GetError().IsEmpty()) {
        NetworkError=CyInsightsTrace::GetError();
        UE_LOG(LogCyInsights,Error,TEXT("%s"),*NetworkError);
    }
    RestoreCaptureNetwork();
    if(GpuCapture)GpuCapture->Stop();
    ConfigureQuestCapture(false);
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
    UE_TRACE_LOG(CyInsights,Sync,CyInsightsSyncChannel)
        << Sync.CpuTicks(CpuTicks) << Sync.ObservedGameFrame(GFrameCounter) << Sync.SessionId(*SessionId);
#endif
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("session_id"),SessionId);
    Row->SetStringField(TEXT("unreal_game_frame"),LexToString(GFrameCounter));
    Row->SetStringField(TEXT("cpu_ticks"),LexToString(CpuTicks));
    Row->SetStringField(TEXT("clock_id"),TEXT("unreal.cpu"));Row->SetStringField(TEXT("status"),TEXT("measured"));
#if CYINSIGHTS_WINDOWS_COLLECTOR
    // A present counter sampled on the game thread is an approximate association.
    // It is never labelled as the GPU frame executed by this game tick.
    if(auto Addon=GetModuleHandleW(L"CyInsightsGpu.addon64")) {
        auto GetStatus=reinterpret_cast<PFN_CyInsightsGpu_GetStatus>(FPlatformProcess::GetDllExport(Addon,TEXT(CYGI_EXPORT_GET_STATUS)));
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
    if(bCapturing&&!CyInsightsTrace::GetError().IsEmpty())StopCapture();
    if(bCapturing&&!NetworkHost.IsEmpty()&&!CyInsightsTrace::IsConnected()) {
        NetworkError=TEXT("Native trace stream interrupted");StopCapture();
    }
    if((bCapturing||bNetworkPending)&&StopAtSeconds>0&&FPlatformTime::Seconds()>=StopAtSeconds){
        StopCapture();if(FParse::Param(FCommandLine::Get(),TEXT("CyInsightsExitAfterCapture")))FPlatformMisc::RequestExit(false);
    }
    return true;
}
bool FCyInsightsModule::CaptureGpu(int32 Frames) {
    auto Options=bCapturing?CaptureOptions:UCyInsightsSettings::CaptureDefaults();Options.gpu_frames=FMath::Clamp(Frames,1,8);Options.gpu_details=true;
    return CaptureGpu(Options);
}
bool FCyInsightsModule::CanCaptureGpu() const {
#if CYINSIGHTS_WINDOWS_COLLECTOR
    auto* GPU=FCyInsightsGpuWindowsModule::Get();return GPU&&GPU->IsAvailable()&&GPU->IsReceiverConnected();
#else
    return false;
#endif
}
bool FCyInsightsModule::CaptureGpu(const cycapture::Options& Requested) {
    auto Policy=Requested;Policy.gpu_details=true;
    const auto Invalid=Policy.Validate();if(!Invalid.empty()){GpuCaptureMessage=UTF8_TO_TCHAR(Invalid.c_str());return false;}
    if(!CanCaptureGpu()){GpuCaptureMessage=TEXT("Detailed GPU capture requires the optional CyInsights Windows collector and a connected CyInsights recorder; remote detailed payload transfer is not available.");UE_LOG(LogCyInsights,Warning,TEXT("%s"),*GpuCaptureMessage);return false;}
#if CYINSIGHTS_WINDOWS_COLLECTOR
    if(!bCapturing){auto Base=Policy;Base.gpu_details=false;if(!StartCapture(FString(),0,&Base))return false;}
    if(auto* GPU=FCyInsightsGpuWindowsModule::Get()){
        FText Message;FCyInsightsGpuWindowsCaptureOptions Options;
        Options.FrameCount=Policy.gpu_frames;Options.bSaveBuffers=Policy.gpu_readbacks;Options.bRecordDescriptors=Policy.gpu_bindings;Options.bRecordBarriers=Policy.gpu_barriers;Options.bTimestampPerCommand=Policy.gpu_per_command;
        const bool Ok=GPU->RequestCapture(Options,Message);GpuCaptureMessage=Message.ToString();UE_LOG(LogCyInsights,Display,TEXT("%s"),*GpuCaptureMessage);return Ok;
    }
#endif
    UE_LOG(LogCyInsights,Warning,TEXT("Detailed GPU commands unavailable on this platform/collector."));return false;
}
TSharedPtr<FJsonObject> FCyInsightsModule::CaptureManifest() const {
    auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("format"),TEXT("cycapture"));Root->SetNumberField(TEXT("version"),1);Root->SetStringField(TEXT("session_id"),SessionId);
    auto Meta=MakeShared<FJsonObject>();Meta->SetStringField(TEXT("project"),FApp::GetProjectName());Meta->SetStringField(TEXT("engine_version"),FEngineVersion::Current().ToString());
    Meta->SetStringField(TEXT("trace_guid_source"),CYINSIGHTS_TRACE_HAS_GUID?TEXT("native"):TEXT("derived: plugin observation epoch"));
    Meta->SetStringField(TEXT("capture_transport"),CYINSIGHTS_TRACE_HAS_AUX_CAPTURE?TEXT("native writer"):TEXT("native UE4 stream through plugin-owned loopback receiver"));
    Meta->SetStringField(TEXT("stop_semantics"),CYINSIGHTS_TRACE_HAS_AUX_CAPTURE?TEXT("native writer stop"):TEXT("receiver cutoff after 100 ms drain, at complete packet boundary; in-flight tail may be unavailable"));
    Meta->SetStringField(TEXT("platform"),FPlatformProperties::PlatformName());Meta->SetStringField(TEXT("graphics_api"),GDynamicRHI?GDynamicRHI->GetName():TEXT("unavailable"));
    Meta->SetStringField(TEXT("gpu"),FPlatformMisc::GetPrimaryGPUBrand());Meta->SetStringField(TEXT("cpu"),FPlatformMisc::GetCPUBrand());Meta->SetStringField(TEXT("created_utc"),CreatedUtc);Root->SetObjectField(TEXT("metadata"),Meta);
    Meta->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());
#if PLATFORM_ANDROID
    // Our own process identity only; no installed-package or filesystem scan.
    if(FILE* Command=std::fopen("/proc/self/cmdline","rb")){char Package[512]{};const auto Size=std::fread(Package,1,sizeof(Package)-1,Command);std::fclose(Command);if(Size&&Package[0]){FString Identity=UTF8_TO_TCHAR(Package);int32 Colon=INDEX_NONE;if(Identity.FindChar(TEXT(':'),Colon))Identity=Identity.Left(Colon);Meta->SetStringField(TEXT("android_package"),Identity);}}
#endif
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
    const bool Current=CyInsightsTrace::GetIdentity(NativeSessionGuid,NativeTraceGuid)&&
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
    Metrics->SetStringField(TEXT("observation_mode"),CaptureOptions.quest_hz==0?TEXT("every game frame"):TEXT("throttled"));
    Metrics->SetStringField(TEXT("observed_frame_count"),LexToString(Current?Status.ObservedFrames:0));
    Sources.Add(MakeShared<FJsonValueObject>(Metrics));
#endif
    auto NativeGpu=Source(TEXT("unreal.gpu.scopes"),GpuCapture&&GpuCapture->Supported()&&GpuCapture->IsRequested()&&GpuCapture->RecordingEnabled()?TEXT("derived"):TEXT("unavailable"),
        TEXT("unreal.utrace"),GpuCapture?GpuCapture->Reason():TEXT("GPU collector unavailable"));
    NativeGpu->SetStringField(TEXT("provider"),TEXT("Native TimingProfiler GPU timelines; per-render-frame instrumented scopes"));
    NativeGpu->SetStringField(TEXT("status_meaning"),TEXT("Capability derived from RHI state; measured scope existence is verified only when analyzing the trace"));
    NativeGpu->SetStringField(TEXT("measurement"),TEXT("GPU timestamps resolved asynchronously by Unreal, not Meta polling or PC receipt time"));
    NativeGpu->SetBoolField(TEXT("rdg_pass_names_requested"),CaptureOptions.gpu_passes);
    NativeGpu->SetBoolField(TEXT("trace_compiled"),FCyInsightsGpuCapture::TraceCompiled());
    NativeGpu->SetBoolField(TEXT("timestamp_queries_available"),GDynamicRHI&&GSupportsTimestampRenderQueries);
    NativeGpu->SetBoolField(TEXT("groups_requested"),GpuCapture&&GpuCapture->IsRequested());
    NativeGpu->SetBoolField(TEXT("group_recording_enabled"),GpuCapture&&GpuCapture->RecordingEnabled());
    NativeGpu->SetNumberField(TEXT("requested_rdg_event_level"),GpuCapture&&GpuCapture->IsRequested()?(CaptureOptions.gpu_passes?3:2):0);
    NativeGpu->SetNumberField(TEXT("applied_rdg_event_level"),GpuCapture?GpuCapture->RecordedEventLevel():0);
    NativeGpu->SetBoolField(TEXT("completion_timestamps_requested"),GpuCapture&&GpuCapture->IsCompletionRequested());
    const int32 TimestampStage=GpuCapture?GpuCapture->RecordedTimestampStage():-1;
    NativeGpu->SetNumberField(TEXT("vulkan_timestamp_stage"),TimestampStage);
    NativeGpu->SetStringField(TEXT("completion_timestamp_path"),GpuCapture?GpuCapture->TimestampPath():TEXT("Unavailable"));
    const bool PassesEnabled=GpuCapture&&GpuCapture->PassesEnabled();
    auto VulkanPasses=Source(TEXT("cyinsights.vulkan.passes"),PassesEnabled&&CyInsightsVulkanPasses::Resolved()?TEXT("measured"):TEXT("unavailable"),TEXT("unreal.utrace"),CyInsightsVulkanPasses::Status());
    VulkanPasses->SetBoolField(TEXT("recording_enabled"),PassesEnabled);
    VulkanPasses->SetStringField(TEXT("resolved_pass_count"),LexToString(PassesEnabled?CyInsightsVulkanPasses::Resolved():uint64(0)));
    VulkanPasses->SetStringField(TEXT("unresolved_pass_count"),LexToString(PassesEnabled?CyInsightsVulkanPasses::Dropped():uint64(0)));
    VulkanPasses->SetStringField(TEXT("pending_pass_count"),LexToString(PassesEnabled?CyInsightsVulkanPasses::Pending():uint64(0)));
    VulkanPasses->SetStringField(TEXT("measurement"),TEXT("BOTTOM_OF_PIPE timestamps outside graphics render passes; original pass extent, ordinal, submission and subpass count. No per-subpass cost or exact raster-unit stage"));
    VulkanPasses->SetStringField(TEXT("alignment"),TEXT("Native engine GPU clock relation when available; approximate unless hardware calibration is independently established. Result receipt is not execution time or game frame identity"));
    VulkanPasses->SetNumberField(TEXT("contents_schema_version"),1);
    VulkanPasses->SetStringField(TEXT("contents"),TEXT("Per-subpass observed direct/indexed/indirect command arguments; secondary buffers included when recorded during capture. No indirect buffer reads, resource copies, individual subpass costs or inferred Unreal names"));
    NativeGpu->SetStringField(TEXT("vulkan_timestamp_stage_meaning"),TimestampStage<0?TEXT("Not applicable or no legacy Vulkan stage control in this RHI"):
        TimestampStage==0?TEXT("TOP_OF_PIPE: command entry; group intervals do not establish completed GPU cost"):
        TEXT("BOTTOM_OF_PIPE: completed work; timestamps inside tile render passes may still not isolate subgroup cost"));
    NativeGpu->SetStringField(TEXT("group_duration_validity"),TEXT("Recorded timestamp differences; near-zero nested intervals do not prove negligible GPU execution cost"));
    NativeGpu->SetStringField(TEXT("cpu_gpu_alignment"),TEXT("Engine/driver dependent: hardware calibration if available, otherwise engine frame alignment may be approximate"));
    NativeGpu->SetStringField(TEXT("hardware_raster_stage"),TEXT("unavailable: group intervals do not identify the exact hardware rasterization stage"));
#if PLATFORM_ANDROID
    NativeGpu->SetStringField(TEXT("timestamp_preparation"),Status.GpuPreparation.IsEmpty()?TEXT("Early Quest module did not report timestamp preparation"):Status.GpuPreparation);
#endif
    NativeGpu->SetStringField(TEXT("coverage"),TEXT("Compiled engine GPU instrumentation only; no command/pipeline/resource interception or compositor internals"));
    NativeGpu->SetStringField(TEXT("completion"),TEXT("In-flight GPU results at trace cutoff can be absent; captured event existence must be checked by the analyzer"));
    Sources.Add(MakeShared<FJsonValueObject>(NativeGpu));
    if(GpuCapture&&GpuCapture->IsCompletionRequested())Sources.Add(MakeShared<FJsonValueObject>(VulkanPasses));
    Sources.Add(MakeShared<FJsonValueObject>(Source(TEXT("gpu.commands"),TEXT("unavailable"),FString(),TEXT("Attach an exported collector capture after GPU completion"))));
    Sources.Add(MakeShared<FJsonValueObject>(Source(TEXT("gpu.cpu_calibration"),TEXT("unavailable"),FString(),TEXT("Collector has no calibrated CPU/GPU clock pairs"))));
    Root->SetStringField(TEXT("requested_native_channels"),CaptureChannels);
    Root->SetObjectField(TEXT("capture_options"),CyInsightsOptions::Write(CaptureOptions));
    if(!GpuCaptureMessage.IsEmpty())Root->SetStringField(TEXT("gpu_capture_message"),GpuCaptureMessage);
    const uint64 StatSamples=StatsCapture?StatsCapture->SampleCount():0;
    auto StatSource=Source(TEXT("unreal.frame_stats"),StatSamples?TEXT("measured"):TEXT("unavailable"),StatSamples?TEXT("unreal.utrace"):FString(),StatSamples?FString():TEXT("Stats disabled or no captured frame observations"));
    StatSource->SetStringField(TEXT("provider"),TEXT("CyInsightsStats v1; Counters observation mirror; native Stats"));
    StatSource->SetStringField(TEXT("sample_count"),LexToString(StatSamples));
    StatSource->SetStringField(TEXT("frame_relation"),TEXT("Source stats frame and game frame boundaries when observed; latest unit/GPU results retain unknown execution-frame identity"));
    Sources.Add(MakeShared<FJsonValueObject>(StatSource));
    Root->SetStringField(TEXT("native_activity_clock"),TEXT("Unreal trace session time; packet contents have bit positions, not execution durations"));
    Root->SetNumberField(TEXT("native_network_verbosity"),CaptureNetworkTraceLevel);
    Root->SetStringField(TEXT("network_capture_requirement"),TEXT("Plugin requests native NetTrace at capture start when compiled and net is selected; -NetTrace=1 also allows startup tracing. No reconstruction of missing traffic"));
    Root->SetStringField(TEXT("memory_capture_requirement"),TEXT("Full allocation tracing requires -trace=memory from startup; memtag requires active LLM"));
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
