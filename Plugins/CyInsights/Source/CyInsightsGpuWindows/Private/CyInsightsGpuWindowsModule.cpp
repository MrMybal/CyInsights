// Reuses CyGPUInspector's in-process capture and viewport integration, without its application/plugin.
// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsightsGpuWindows.h"
#include "CyInsightsGpuViewExtension.h"
#include "ICyInsightsGpuLoader.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersionComparison.h"
#include "SceneViewExtension.h"
#include "UnrealClient.h"
#include "HAL/PlatformTime.h"
#include "Modules/ModuleManager.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "CyInsightsGpuApi.h"

DEFINE_LOG_CATEGORY_STATIC(LogCyInsightsWindowsGpu,Log,All);
namespace {
auto& PostEngineInit() {
#if UE_VERSION_OLDER_THAN(5,8,0)
    return FCoreDelegates::OnPostEngineInit;
#else
    return FCoreDelegates::GetOnPostEngineInit();
#endif
}
bool ReadStatus(void* Function,CygiStatus& Status) {
    FMemory::Memzero(Status);Status.size=sizeof(Status);
    return Function && reinterpret_cast<PFN_CyInsightsGpu_GetStatus>(Function)(&Status)==1 &&
        Status.api_version==CYGI_INPROCESS_API_VERSION;
}
}
FCyInsightsGpuWindowsModule* FCyInsightsGpuWindowsModule::Get() {
    return FModuleManager::GetModulePtr<FCyInsightsGpuWindowsModule>(TEXT("CyInsightsGpuWindows"));
}
void FCyInsightsGpuWindowsModule::StartupModule() {
    const auto* Loader=ICyInsightsGpuLoader::Get();
    // An installed but disabled collector creates no ticker or viewport markers.
    if(!Loader || !Loader->IsEnabled())return;
    TickHandle=FCyInsightsGpuTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this,&FCyInsightsGpuWindowsModule::Tick),.1f);
    if(GEngine)RegisterViewExtension();
    else PostEngineInitHandle=PostEngineInit().AddRaw(this,&FCyInsightsGpuWindowsModule::RegisterViewExtension);
    UE_LOG(LogCyInsightsWindowsGpu,Display,TEXT("%s"),*Loader->GetStateText());
}
void FCyInsightsGpuWindowsModule::ShutdownModule() {
    PostEngineInit().Remove(PostEngineInitHandle);
    ViewExtension.Reset();
    if(TickHandle.IsValid()) {FCyInsightsGpuTicker::GetCoreTicker().RemoveTicker(TickHandle);TickHandle.Reset();}
}
void FCyInsightsGpuWindowsModule::RegisterViewExtension() {
    if(GEngine && !ViewExtension.IsValid())ViewExtension=FSceneViewExtensions::NewExtension<FCyInsightsGpuViewExtension>();
}
FViewport* FCyInsightsGpuWindowsModule::GetMainViewport() const {
    if(MainViewportResolver)return MainViewportResolver();
    return GEngine && GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
}
bool FCyInsightsGpuWindowsModule::ResolveAddon() const {
    const auto* Loader=ICyInsightsGpuLoader::Get();
    HMODULE Addon=Loader && Loader->IsEnabled() ? GetModuleHandleW(TEXT("CyInsightsGpu.addon64")) : nullptr;
    // Refresh on every request: ReShade may unload/reload the add-on during device creation.
    StatusFunction=Addon ? reinterpret_cast<void*>(GetProcAddress(Addon,CYGI_EXPORT_GET_STATUS)) : nullptr;
    CaptureFunction=Addon ? reinterpret_cast<void*>(GetProcAddress(Addon,CYGI_EXPORT_REQUEST_CAPTURE)) : nullptr;
    return StatusFunction && CaptureFunction;
}
bool FCyInsightsGpuWindowsModule::IsAvailable() const {
    CygiStatus Status;return ResolveAddon() && ReadStatus(StatusFunction,Status) && Status.device_count!=0;
}
bool FCyInsightsGpuWindowsModule::IsReceiverConnected() const {
    CygiStatus Status;return ResolveAddon() && ReadStatus(StatusFunction,Status) && Status.standalone_connected!=0;
}
FText FCyInsightsGpuWindowsModule::GetStatusText() const {
    const auto* Loader=ICyInsightsGpuLoader::Get();
    if(!IsAvailable())return FText::FromString(Loader ? Loader->GetStateText() : TEXT("CyInsights Windows collector unavailable"));
    return FText::FromString(IsReceiverConnected() ? TEXT("CyInsights Windows collector ready; CyInsights recorder connected") :
        TEXT("CyInsights Windows collector ready; connect its local process from CyInsights > Session > Windows collector"));
}
bool FCyInsightsGpuWindowsModule::RequestCapture(const FCyInsightsGpuWindowsCaptureOptions& Options,FText& Message) {
    CygiStatus Before;
    if(!ResolveAddon() || !ReadStatus(StatusFunction,Before) || !Before.device_count || !Before.standalone_connected) {
        Message=GetStatusText();return false;
    }
    if(bWatchingCapture || Before.capture_stage==1 || Before.capture_stage==2) {
        Message=FText::FromString(TEXT("A detailed GPU capture is already pending"));return false;
    }
    CygiCaptureOptions Request{};Request.size=sizeof(Request);
    Request.frame_count=static_cast<uint32_t>(FMath::Clamp(Options.FrameCount,1,8));
    Request.include_bindings=Options.bRecordDescriptors;Request.include_barriers=Options.bRecordBarriers;
    Request.per_draw_timing=Options.bTimestampPerCommand;Request.include_buffers=Options.bSaveBuffers;
    if(reinterpret_cast<PFN_CyInsightsGpu_RequestCapture>(CaptureFunction)(&Request)!=1) {
        Message=FText::FromString(TEXT("CyInsights collector refused the capture"));return false;
    }
    if(auto* Viewport=GetMainViewport())Viewport->Invalidate();
    PreviousFirstFrame=Before.capture_first_frame;bWatchingCapture=true;WatchingSince=FPlatformTime::Seconds();
    Message=FText::FromString(TEXT("Optional detailed GPU capture armed; CyInsights receives GPU results asynchronously"));return true;
}
bool FCyInsightsGpuWindowsModule::Tick(float) {
    bAddonPresent=ResolveAddon();
    if(bWatchingCapture) {
        CygiStatus Status;
        if(ReadStatus(StatusFunction,Status) && Status.capture_first_frame!=PreviousFirstFrame && Status.capture_stage==3) {
            bWatchingCapture=false;
            UE_LOG(LogCyInsightsWindowsGpu,Display,TEXT("Detailed GPU frame recorded; CyInsights saves it when timestamps arrive"));
        } else if((ReadStatus(StatusFunction,Status) && Status.capture_stage==4) || FPlatformTime::Seconds()-WatchingSince>60) {
            bWatchingCapture=false;
            UE_LOG(LogCyInsightsWindowsGpu,Warning,TEXT("Detailed GPU capture failed or timed out"));
        }
    }
    return true;
}
IMPLEMENT_MODULE(FCyInsightsGpuWindowsModule,CyInsightsGpuWindows)
