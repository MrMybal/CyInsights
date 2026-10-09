// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CoreMinimal.h"
#include "CyInsightsTrace.h"
#include "ICyInsightsQuestMetrics.h"
#if CYINSIGHTS_QUEST_METRICS_API
#include "MetaMetricsReader.h"
#include "IOpenXRExtensionPlugin.h"
#include "IOpenXRHMDModule.h"
#include "OpenXRCore.h"
#endif
#include "Containers/Ticker.h"
#include "Features/IModularFeatures.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Trace/Trace.inl"

DEFINE_LOG_CATEGORY_STATIC(LogCyInsightsQuest, Log, All);
#if COUNTERSTRACE_ENABLED && CYINSIGHTS_QUEST_METRICS_API
static TAutoConsoleVariable<int32> CVarQuestMetrics(
    TEXT("CyInsights.QuestGpuMetrics"), 1,
    TEXT("Embed available Meta OpenXR performance counters into the active Unreal trace. 0=disabled, 1=enabled."));
static TAutoConsoleVariable<float> CVarQuestMetricsHz(
    TEXT("CyInsights.QuestGpuMetricsHz"), 10.0f,
    TEXT("Meta metric polling rate (1..120 Hz). Runtime measurement intervals remain unknown; polling is not per-frame GPU timing."));

// Alongside standard Counters provider events, keep source/units/validity and
// observation identity inside the SAME trace. No CSV or privileged shell tool.
UE_TRACE_EVENT_BEGIN(CyInsightsQuest, Source)
    UE_TRACE_EVENT_FIELD(uint64, Cycle)
    UE_TRACE_EVENT_FIELD(uint32, SchemaVersion)
    UE_TRACE_EVENT_FIELD(uint32, CounterCount)
    UE_TRACE_EVENT_FIELD(uint64, InstanceGeneration)
    UE_TRACE_EVENT_FIELD(uint64, SessionGeneration)
    UE_TRACE_EVENT_FIELD(CyInsightsNativeTrace::WideString, State)
    UE_TRACE_EVENT_FIELD(CyInsightsNativeTrace::WideString, Clock)
    UE_TRACE_EVENT_FIELD(CyInsightsNativeTrace::WideString, TimestampMeaning)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(CyInsightsQuest, Sample)
    UE_TRACE_EVENT_FIELD(uint64, ReadBeginCycle)
    UE_TRACE_EVENT_FIELD(uint64, ReadEndCycle)
    UE_TRACE_EVENT_FIELD(uint64, ObservedGameFrame)
    UE_TRACE_EVENT_FIELD(uint64, PathId)
    UE_TRACE_EVENT_FIELD(uint64, InstanceGeneration)
    UE_TRACE_EVENT_FIELD(uint64, SessionGeneration)
    UE_TRACE_EVENT_FIELD(CyInsightsNativeTrace::WideString, Path)
    UE_TRACE_EVENT_FIELD(int32, Result)
    UE_TRACE_EVENT_FIELD(int32, Unit)
    UE_TRACE_EVENT_FIELD(uint64, RawFlags)
    UE_TRACE_EVENT_FIELD(uint32, UIntValue)
    UE_TRACE_EVENT_FIELD(float, FloatValue)
    UE_TRACE_EVENT_FIELD(uint8, ValidUInt)
    UE_TRACE_EVENT_FIELD(uint8, ValidFloat)
UE_TRACE_EVENT_END()

class FCyInsightsQuestModule final : public IModuleInterface,
    public IOpenXRExtensionPlugin, public ICyInsightsQuestMetrics
{
public:
    void StartupModule() override
    {
        RegisterOpenXRExtensionModularFeature();
        IModularFeatures::Get().RegisterModularFeature(FeatureName(), static_cast<ICyInsightsQuestMetrics*>(this));
        TickHandle = FCyInsightsTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FCyInsightsQuestModule::Tick));
    }
    void ShutdownModule() override
    {
        FCyInsightsTicker::GetCoreTicker().RemoveTicker(TickHandle);
        UnregisterOpenXRExtensionModularFeature();
        IModularFeatures::Get().UnregisterModularFeature(FeatureName(), static_cast<ICyInsightsQuestMetrics*>(this));
        FScopeLock Lock(&Mutex);
        Reader.End();
        Session = XR_NULL_HANDLE;
    }
    FString GetDisplayName() override { return TEXT("CyInsights Quest GPU metrics"); }
    bool GetOptionalExtensions(TArray<const ANSICHAR*>& Extensions) override
    {
        // Optional even when recording is off: the option can change at runtime.
        // An unsupported extension must never prevent XR initialization.
        Extensions.Add(XR_META_PERFORMANCE_METRICS_EXTENSION_NAME);
        return true;
    }
    const void* OnCreateInstance(IOpenXRHMDModule* Module, const void* Next) override
    {
        HmdModule = Module;
        return Next;
    }
    void PostCreateInstance(XrInstance Instance) override
    {
        FScopeLock Lock(&Mutex);
        ++InstanceGeneration;
        WrittenState.Reset();
        const bool Enabled = HmdModule && HmdModule->IsExtensionEnabled(
            UTF8_TO_TCHAR(XR_META_PERFORMANCE_METRICS_EXTENSION_NAME));
        if (!Enabled || !Reader.Initialize(OpenXRDynamicAPI::xrGetInstanceProcAddr, Instance)) {
            Reason = Enabled ? UTF8_TO_TCHAR(Reader.Error().c_str()) :
                TEXT("XR_META_performance_metrics unavailable or not enabled by the active XR backend");
            UE_LOG(LogCyInsightsQuest, Display, TEXT("%s"), *Reason);
        } else {
            Reason = TEXT("Waiting for a trace and an OpenXR session");
        }
    }
    void PostCreateSession(XrSession InSession) override
    {
        FScopeLock Lock(&Mutex);
        Reader.End();
        Session = InSession;
        ++SessionGeneration;
        WrittenState.Reset();
    }
    void OnDestroySession(XrSession InSession) override
    {
        FScopeLock Lock(&Mutex);
        if (Session == InSession) {
            // Unreal calls this before xrDestroySession; never retain its handle.
            Reader.End();
            Session = XR_NULL_HANDLE;
            Reason = TEXT("OpenXR session ended");
        }
    }
    FCyInsightsQuestMetricsStatus GetMetricsStatus() const override
    {
        FScopeLock Lock(&Mutex);
        FCyInsightsQuestMetricsStatus Status;
        Status.Reason = Reason;
        Status.TraceGuid = TraceGuid.ToString(EGuidFormats::DigitsWithHyphens);
        Status.ValidSamples = ValidSamples;
        Status.CounterCount = static_cast<int32>(Reader.Metrics().size());
        return Status;
    }
    void StopMetrics() override
    {
        FScopeLock Lock(&Mutex);
        Reader.End();
    }

private:
    bool Tick(float)
    {
        FScopeLock Lock(&Mutex);
        FGuid NativeSessionGuid, CurrentTraceGuid;
        const bool Connected = CyInsightsTrace::GetIdentity(NativeSessionGuid, CurrentTraceGuid);
        if (Connected && TraceGuid != CurrentTraceGuid) {
            TraceGuid = CurrentTraceGuid;
            ValidSamples = 0;
            WrittenState.Reset();
            LastPollSeconds = 0;
        }
        const bool Recording = Connected && CVarQuestMetrics.GetValueOnAnyThread() != 0 &&
            UE_TRACE_CHANNELEXPR_IS_ENABLED(CountersChannel);
        if (!Recording) {
            Reader.End();
            if (Connected) Reason = TEXT("Meta metric option or counters trace channel disabled");
            return true;
        }
        const double Now = FPlatformTime::Seconds();
        float Rate = CVarQuestMetricsHz.GetValueOnAnyThread();
        if (!FMath::IsFinite(Rate)) Rate = 10.0f;
        if (Now - LastPollSeconds < 1.0 / FMath::Clamp(Rate, 1.0f, 120.0f)) return true;
        LastPollSeconds = Now;
        if (!Reader.Ready()) {
            // Keep the startup failure; a supported catalog may be empty.
            if (Reason.IsEmpty()) Reason = TEXT("Meta OpenXR performance counters unavailable");
            EmitSource();
            return true;
        }
        if (!Reader.Begin(Session)) {
            Reason = Session == XR_NULL_HANDLE ? TEXT("No active OpenXR session") :
                UTF8_TO_TCHAR(Reader.Error().c_str());
            EmitSource();
            return true;
        }
        Reason = TEXT("Polling latest runtime values; each sample carries validity; measurement interval and GPU execution frame unavailable");
        EmitSource();
        for (const auto& Metric : Reader.Metrics()) {
            const uint64 BeginCycle = FPlatformTime::Cycles64();
            const auto Reading = Reader.Read(Metric.path);
            const uint64 EndCycle = FPlatformTime::Cycles64();
            const FString Path = UTF8_TO_TCHAR(Metric.name.c_str());
            UE_TRACE_LOG(CyInsightsQuest, Sample, CountersChannel)
                << Sample.ReadBeginCycle(BeginCycle) << Sample.ReadEndCycle(EndCycle)
                << Sample.ObservedGameFrame(GFrameCounter) << Sample.PathId(Metric.path)
                << Sample.InstanceGeneration(InstanceGeneration) << Sample.SessionGeneration(SessionGeneration)
                << Sample.Path(*Path) << Sample.Result(static_cast<int32>(Reading.result))
                << Sample.Unit(static_cast<int32>(Reading.raw.counterUnit))
                << Sample.RawFlags(Reading.raw.counterFlags)
                << Sample.UIntValue(Reading.raw.uintValue) << Sample.FloatValue(Reading.raw.floatValue)
                << Sample.ValidUInt(Reading.valid_uint) << Sample.ValidFloat(Reading.valid_float);

            const FString Name = Path.StartsWith(TEXT("unresolved_path_")) ?
                FString::Printf(TEXT("CyInsights/Meta/instance_%llu/%s"), static_cast<unsigned long long>(InstanceGeneration), *Path) :
                TEXT("CyInsights/Meta") + Path;
            // Explicit availability companion: no synthetic zero GPU durations.
            SetInt(Name + TEXT("/available"), Reading.Valid() ? 1 : 0);
            SetInt(Name + TEXT("/uint/available"), Reading.valid_uint ? 1 : 0);
            SetInt(Name + TEXT("/float/available"), Reading.valid_float ? 1 : 0);
            if (Reading.Valid()) ++ValidSamples;
            FString Unit = UTF8_TO_TCHAR(cyinsights::quest::UnitName(Reading.raw.counterUnit));
            if (Unit == TEXT("unknown")) Unit += FString::Printf(TEXT("_%d"), static_cast<int32>(Reading.raw.counterUnit));
            if (Reading.valid_uint) SetInt(Name + TEXT(" [") + Unit + TEXT("]/uint"), Reading.raw.uintValue);
            if (Reading.valid_float) SetFloat(Name + TEXT(" [") + Unit + TEXT("]/float"), Reading.raw.floatValue);
        }
        return true;
    }
    void EmitSource()
    {
        if (WrittenState == Reason) return;
        WrittenState = Reason;
        UE_TRACE_LOG(CyInsightsQuest, Source, CountersChannel)
            << Source.Cycle(FPlatformTime::Cycles64()) << Source.SchemaVersion(1)
            << Source.CounterCount(static_cast<uint32>(Reader.Metrics().size()))
            << Source.InstanceGeneration(InstanceGeneration) << Source.SessionGeneration(SessionGeneration)
            << Source.State(*Reason) << Source.Clock(TEXT("unreal.cpu"))
            << Source.TimestampMeaning(TEXT("observation; runtime measurement interval and GPU execution frame unknown"));
    }
    void SetInt(const FString& Name, int64 Value)
    {
        uint16& Id = CounterIds.FindOrAdd(Name);
        if (!Id) Id = FCountersTrace::OutputInitCounter(*Name, TraceCounterType_Int, TraceCounterDisplayHint_None);
        FCountersTrace::OutputSetValue(Id, Value);
    }
    void SetFloat(const FString& Name, double Value)
    {
        uint16& Id = CounterIds.FindOrAdd(Name);
        if (!Id) Id = FCountersTrace::OutputInitCounter(*Name, TraceCounterType_Float, TraceCounterDisplayHint_None);
        FCountersTrace::OutputSetValue(Id, Value);
    }
    mutable FCriticalSection Mutex;
    cyinsights::quest::MetricsReader Reader;
    IOpenXRHMDModule* HmdModule = nullptr;
    XrSession Session = XR_NULL_HANDLE;
    FCyInsightsTickerHandle TickHandle;
    TMap<FString, uint16> CounterIds;
    FGuid TraceGuid;
    uint64 ValidSamples = 0;
    uint64 InstanceGeneration = 0, SessionGeneration = 0;
    double LastPollSeconds = 0;
    FString Reason = TEXT("Waiting for the Unreal OpenXR backend to initialize");
    FString WrittenState;
};
IMPLEMENT_MODULE(FCyInsightsQuestModule, CyInsightsQuest)
#else
class FCyInsightsQuestModule final : public IModuleInterface, public ICyInsightsQuestMetrics
{
public:
    void StartupModule() override
    {
        IModularFeatures::Get().RegisterModularFeature(FeatureName(), static_cast<ICyInsightsQuestMetrics*>(this));
        UE_LOG(LogCyInsightsQuest, Display, TEXT("%s"), *GetMetricsStatus().Reason);
    }
    void ShutdownModule() override
    {
        IModularFeatures::Get().UnregisterModularFeature(FeatureName(), static_cast<ICyInsightsQuestMetrics*>(this));
    }
    FCyInsightsQuestMetricsStatus GetMetricsStatus() const override
    {
        FCyInsightsQuestMetricsStatus Status;
#if !CYINSIGHTS_QUEST_METRICS_API
        Status.Reason = TEXT("Quest metrics unavailable: this engine's OpenXR SDK/lifecycle lacks the required Meta metric API or destruction hook");
#else
        Status.Reason = TEXT("Quest metrics unavailable: this target disables Unreal Counters tracing");
#endif
        return Status;
    }
    void StopMetrics() override {}
};
IMPLEMENT_MODULE(FCyInsightsQuestModule, CyInsightsQuest)
#endif
