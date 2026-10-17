// Derived capture integration from CyGPUInspector. Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"
#include "Templates/Function.h"
#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION >= 5
using FCyInsightsGpuTicker=FTSTicker;
using FCyInsightsGpuTickerHandle=FTSTicker::FDelegateHandle;
#else
using FCyInsightsGpuTicker=FTicker;
using FCyInsightsGpuTickerHandle=FDelegateHandle;
#endif
class FViewport;
struct FCyInsightsGpuWindowsCaptureOptions
{
    int32 FrameCount=1;
    bool bSaveBuffers=false, bRecordDescriptors=false, bRecordBarriers=false, bTimestampPerCommand=false;
};
class CYINSIGHTSGPUWINDOWS_API FCyInsightsGpuWindowsModule : public IModuleInterface
{
public:
    static FCyInsightsGpuWindowsModule* Get();
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    bool IsAvailable() const;
    bool IsReceiverConnected() const;
    bool IsAddonPresent() const { return bAddonPresent; }
    FViewport* GetMainViewport() const;
    void SetMainViewportResolver(TFunction<FViewport*()> Resolver) { MainViewportResolver=MoveTemp(Resolver); }
    bool RequestCapture(const FCyInsightsGpuWindowsCaptureOptions& Options,FText& Message);
    FText GetStatusText() const;
private:
    bool ResolveAddon() const;
    bool Tick(float);
    void RegisterViewExtension();
    FCyInsightsGpuTickerHandle TickHandle;
    FDelegateHandle PostEngineInitHandle;
    TFunction<FViewport*()> MainViewportResolver;
    TSharedPtr<class FCyInsightsGpuViewExtension,ESPMode::ThreadSafe> ViewExtension;
    mutable void* StatusFunction=nullptr;
    mutable void* CaptureFunction=nullptr;
    bool bAddonPresent=false, bWatchingCapture=false;
    double WatchingSince=0;
    uint64 PreviousFirstFrame=0;
};
