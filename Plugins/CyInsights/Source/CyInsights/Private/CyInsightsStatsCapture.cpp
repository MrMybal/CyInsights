// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsightsStatsCapture.h"
#include "CyInsightsTrace.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "ProfilingDebugging/CountersTrace.h"
#if __has_include("RenderTimer.h")
#include "RenderTimer.h"
#else
#include "RenderCore.h"
#endif
#include "DynamicRHI.h"
#include "Stats/Stats.h"
#include "Stats/StatsData.h"
#include "UObject/UObjectArray.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Trace/Trace.inl"
#if STATS && CYINSIGHTS_STATS_USE_PIPE
#include "Tasks/Pipe.h"
extern CORE_API UE::Tasks::FPipe GStatsPipe;
#if ENGINE_MINOR_VERSION >= 7
#include "Stats/StatsSystem.h"
#endif
#elif STATS
#include "Async/TaskGraphInterfaces.h"
#endif

static TAutoConsoleVariable<int32> CVarRecordStats(TEXT("CyInsights.RecordStats"),1,TEXT("Record frame observations and enabled Unreal stat groups in the coordinated trace"));
static TAutoConsoleVariable<int32> CVarRecordGroups(TEXT("CyInsights.RecordStatGroups"),0,TEXT("Explicitly enable additional Unreal stat-group aggregation during the next capture. Default: basic observations only."));
static TAutoConsoleVariable<FString> CVarStatsGroups(TEXT("CyInsights.StatsGroups"),TEXT("Game,UObjects,GPU"),TEXT("Selected active stat groups when RecordStatGroups is enabled: * or comma-separated names."));
UE_TRACE_CHANNEL_DEFINE(CyInsightsStatsChannel);
UE_TRACE_EVENT_BEGIN(CyInsightsStats,Spec,NoSync|Important)
    UE_TRACE_EVENT_FIELD(uint32,Id)
    UE_TRACE_EVENT_FIELD(bool,Floating)
    UE_TRACE_EVENT_FIELD(CyInsightsNativeTrace::WideString,Group)
    UE_TRACE_EVENT_FIELD(CyInsightsNativeTrace::WideString,Name)
    UE_TRACE_EVENT_FIELD(CyInsightsNativeTrace::WideString,Unit)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(CyInsightsStats,Sample)
    UE_TRACE_EVENT_FIELD(uint32,Id)
    UE_TRACE_EVENT_FIELD(uint64,BeginCycles)
    UE_TRACE_EVENT_FIELD(uint64,EndCycles)
    UE_TRACE_EVENT_FIELD(uint64,ObservedCycles)
    UE_TRACE_EVENT_FIELD(uint64,GameFrame)
    UE_TRACE_EVENT_FIELD(int64,StatsFrame)
    UE_TRACE_EVENT_FIELD(uint32,Flags)
    UE_TRACE_EVENT_FIELD(int64,Integer)
    UE_TRACE_EVENT_FIELD(double,Real)
UE_TRACE_EVENT_END()
namespace {
// Flags shared with the desktop cache: bounded frame, delayed result,
// execution frame unknown, derived aggregate, valid reading.
constexpr uint32 Bounded=1,Delayed=2,ExecutionUnknown=4,Aggregate=8,Valid=16;
void OnStatsContext(TFunction<void()> Work) {
#if STATS && CYINSIGHTS_STATS_USE_PIPE
    GStatsPipe.Launch(UE_SOURCE_LOCATION,MoveTemp(Work)).Wait();
#elif STATS
    if(!FPlatformProcess::SupportsMultithreading()){Work();return;}
    auto Task=FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(FSimpleDelegateGraphTask::FDelegate::CreateLambda(MoveTemp(Work)),TStatId(),nullptr,ENamedThreads::StatsThread);
    FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);
#endif
}
int64 CurrentStatsFrame() {
#if STATS && ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
    return UE::Stats::FStats::GameThreadStatsFrame;
#elif STATS
    return FStats::GameThreadStatsFrame;
#else
    return -1;
#endif
}
}
FCyInsightsStatsCapture::FCyInsightsStatsCapture() {
    BeginHandle=FCoreDelegates::OnBeginFrame.AddRaw(this,&FCyInsightsStatsCapture::BeginFrame);
    EndHandle=FCoreDelegates::OnEndFrame.AddRaw(this,&FCyInsightsStatsCapture::EndFrame);
#if STATS
    OnStatsContext([this]{StatsHandle=FStatsThreadState::GetLocalState().NewFrameDelegate.AddRaw(this,&FCyInsightsStatsCapture::StatsFrame);});
#endif
}
FCyInsightsStatsCapture::~FCyInsightsStatsCapture() {
    Stop();FCoreDelegates::OnBeginFrame.Remove(BeginHandle);FCoreDelegates::OnEndFrame.Remove(EndHandle);
#if STATS
    OnStatsContext([this]{FStatsThreadState::GetLocalState().NewFrameDelegate.Remove(StatsHandle);});
#endif
}
void FCyInsightsStatsCapture::Start(const cycapture::Options& Options) {
    FScopeLock Lock(&Mutex);if(Recording)return;
    Samples=0;Frames.Reset();BeginCycles=0;Recording=Options.frame_stats;
    RecordGroups=Options.stat_groups_enabled;RecordObjects=Options.object_counts;RecordMemory=Options.memory_stats;
    FirstStatsFrame=CurrentStatsFrame()+1;
    FString Choice=UTF8_TO_TCHAR(Options.stat_groups.c_str());Choice.TrimStartAndEndInline();AllGroups=Choice==TEXT("*");SelectedGroups.Reset();
    TArray<FString> Names;Choice.ParseIntoArray(Names,TEXT(","),true);
    for(FString Name:Names){Name.TrimStartAndEndInline();Name.RemoveFromStart(TEXT("STATGROUP_"));SelectedGroups.Add(FName(*(TEXT("STATGROUP_")+Name)));}
    for(auto& Pair:Metrics)Pair.Value.Declared=false;
#if STATS
    if(Recording&&RecordGroups){
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        StatsPrimaryEnableAdd();
#else
        StatsMasterEnableAdd();
#endif
    }
#endif
}
void FCyInsightsStatsCapture::Stop() {
    FScopeLock Lock(&Mutex);if(!Recording)return;Recording=false;
#if STATS
    if(RecordGroups){
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    StatsPrimaryEnableSubtract();
#else
    StatsMasterEnableSubtract();
#endif
    }
#endif
}
uint64 FCyInsightsStatsCapture::SampleCount() const {FScopeLock Lock(&Mutex);return Samples;}
void FCyInsightsStatsCapture::BeginFrame(){FScopeLock Lock(&Mutex);if(Recording)BeginCycles=FPlatformTime::Cycles64();}
void FCyInsightsStatsCapture::Emit(const FString& Group,const FString& Name,const TCHAR* Unit,bool Floating,int64 Integer,double Real,const FFrame& Frame,uint32 Flags) {
    const FString Label=TEXT("CyInsights/Stats/")+Group+TEXT("/")+Name+TEXT(" [")+Unit+TEXT("]");
    FMetric* Metric=Metrics.Find(Label);
    if(!Metric){FMetric New;New.Id=Metrics.Num();New.Floating=Floating;
#if COUNTERSTRACE_ENABLED
        New.Counter=FCountersTrace::OutputInitCounter(*Label,Floating?TraceCounterType_Float:TraceCounterType_Int,TraceCounterDisplayHint_None);
#endif
        Metric=&Metrics.Add(Label,New);
    }
    // Emit declarations in every capture: late connection must not depend on
    // cached metadata from a preceding session or an enabled channel at startup.
    if(!Metric->Declared){
        UE_TRACE_LOG(CyInsightsStats,Spec,CyInsightsStatsChannel,(Group.Len()+Name.Len()+FCString::Strlen(Unit))*sizeof(TCHAR))
            << Spec.Id(Metric->Id) << Spec.Floating(Floating) << Spec.Group(*Group) << Spec.Name(*Name) << Spec.Unit(Unit);
        Metric->Declared=true;
    }
    const uint64 Observed=FPlatformTime::Cycles64();
    UE_TRACE_LOG(CyInsightsStats,Sample,CyInsightsStatsChannel)
        << Sample.Id(Metric->Id) << Sample.BeginCycles(Frame.Begin) << Sample.EndCycles(Frame.End)
        << Sample.ObservedCycles(Observed) << Sample.GameFrame(Frame.Game) << Sample.StatsFrame(Frame.Stats)
        << Sample.Flags(Flags) << Sample.Integer(Integer) << Sample.Real(Real);
#if COUNTERSTRACE_ENABLED
    if(Flags&Valid){if(Floating)FCountersTrace::OutputSetValue(Metric->Counter,Real);else FCountersTrace::OutputSetValue(Metric->Counter,Integer);}
#endif
    ++Samples;
}
void FCyInsightsStatsCapture::EndFrame() {
    FScopeLock Lock(&Mutex);if(!Recording)return;
    const FFrame Frame{CurrentStatsFrame(),GFrameCounter,BeginCycles,FPlatformTime::Cycles64()};
    const uint32 Bounds=BeginCycles?Bounded:0;
    if(BeginCycles&&RecordGroups){Frames.Add(Frame);if(Frames.Num()>256)Frames.RemoveAt(0,Frames.Num()-256);}
    const double Ms=FPlatformTime::GetSecondsPerCycle()*1000.;
    // These are the engine's latest resolved stat-unit observations. Render/RHI
    // and GPU execution-frame identity is not supplied by these public APIs.
    Emit(TEXT("Unit"),TEXT("GameThread latest"),TEXT("ms"),true,0,GGameThreadTime*Ms,Frame,Bounds|ExecutionUnknown|(GGameThreadTime?Valid:0));
    Emit(TEXT("Unit"),TEXT("RenderThread latest"),TEXT("ms"),true,0,GRenderThreadTime*Ms,Frame,Bounds|Delayed|ExecutionUnknown|(GRenderThreadTime?Valid:0));
    Emit(TEXT("Unit"),TEXT("RHIThread latest"),TEXT("ms"),true,0,GRHIThreadTime*Ms,Frame,Bounds|Delayed|ExecutionUnknown|(GRHIThreadTime?Valid:0));
    const uint32 GpuCycles=GDynamicRHI?RHIGetGPUFrameCycles():0;
    Emit(TEXT("GPU"),TEXT("GPU frame latest"),TEXT("ms"),true,0,GpuCycles*Ms,Frame,Bounds|Delayed|ExecutionUnknown|(GpuCycles?Valid:0));
    if(BeginCycles)Emit(TEXT("Unit"),TEXT("Observed frame span"),TEXT("ms"),true,0,(Frame.End-Frame.Begin)*FPlatformTime::GetSecondsPerCycle64()*1000.,Frame,Bounds|Valid);
    if(RecordObjects){
    Emit(TEXT("UObjects"),TEXT("Allocated object slots"),TEXT("objects"),false,GUObjectArray.GetObjectArrayNumMinusAvailable(),0,Frame,Bounds|Valid);
    Emit(TEXT("UObjects"),TEXT("Object array slots"),TEXT("slots"),false,GUObjectArray.GetObjectArrayNum(),0,Frame,Bounds|Valid);
    }
    if(RecordMemory){
    const auto Memory=FPlatformMemory::GetStats();
    Emit(TEXT("Memory"),TEXT("Process physical memory"),TEXT("bytes"),false,static_cast<int64>(Memory.UsedPhysical),0,Frame,Bounds|Valid);
    Emit(TEXT("Memory"),TEXT("Process virtual memory"),TEXT("bytes"),false,static_cast<int64>(Memory.UsedVirtual),0,Frame,Bounds|Valid);
    }
}
void FCyInsightsStatsCapture::StatsFrame(int64 SourceFrame) {
#if STATS
    FScopeLock Lock(&Mutex);if(!Recording||!RecordGroups||SourceFrame<FirstStatsFrame)return;
    auto& State=FStatsThreadState::GetLocalState();if(!State.IsFrameValid(SourceFrame))return;
    FFrame Frame;Frame.Stats=SourceFrame;Frame.Game=MAX_uint64;
    for(int32 i=Frames.Num()-1;i>=0;--i)if(Frames[i].Stats==SourceFrame){Frame=Frames[i];break;}
    TArray<FStatMessage> Inclusive,Exclusive;
    State.GetInclusiveAggregateStackStats(SourceFrame,Inclusive,nullptr,true);
    State.GetExclusiveAggregateStackStats(SourceFrame,Exclusive,nullptr,false);
    auto Write=[&](const FStatMessage& Stat,bool IsExclusive){
        const FName GroupName=Stat.NameAndInfo.GetGroupName();if(!AllGroups&&!SelectedGroups.Contains(GroupName))return;
        FString Group=GroupName.ToString();
        Group.RemoveFromStart(TEXT("STATGROUP_"));const FString Name=Stat.NameAndInfo.GetShortName().ToString();
        const uint32 Flags=(Frame.Begin?Bounded:0)|Delayed|Valid;
        if(Stat.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle)){
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
            const uint32 Extra=Stat.NameAndInfo.GetFlag(EStatMetaFlags::IsGPU)?ExecutionUnknown:0;
#else
            const uint32 Extra=(Group.Equals(TEXT("GPU"),ESearchCase::IgnoreCase)||Group.Equals(TEXT("GPUStats"),ESearchCase::IgnoreCase))?ExecutionUnknown:0;
#endif
            Emit(Group,Name+(IsExclusive?TEXT(" exclusive"):TEXT(" inclusive")),TEXT("ms"),true,0,Stat.GetValue_Duration()*FPlatformTime::GetSecondsPerCycle()*1000.,Frame,Flags|Aggregate|Extra);
            if(!IsExclusive&&Stat.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))Emit(Group,Name+TEXT(" calls"),TEXT("calls"),false,Stat.GetValue_CallCount(),0,Frame,Flags|Aggregate|Extra);
        }else if(!IsExclusive&&Stat.NameAndInfo.GetField<EStatDataType>()==EStatDataType::ST_int64)
            Emit(Group,Name,Stat.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory)?TEXT("bytes"):TEXT("count"),false,Stat.GetValue_int64(),0,Frame,Flags);
        else if(!IsExclusive&&Stat.NameAndInfo.GetField<EStatDataType>()==EStatDataType::ST_double){const double Value=Stat.GetValue_double();Emit(Group,Name,TEXT("value"),true,0,Value,Frame,FMath::IsFinite(Value)?Flags:Flags&~Valid);}
    };
    for(const auto& Stat:Inclusive)Write(Stat,false);
    for(const auto& Stat:Exclusive)Write(Stat,true);
#endif
}
