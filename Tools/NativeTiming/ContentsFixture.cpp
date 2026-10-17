// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
// Synthetic provider integration fixture. Never describes actual GPU work.
#include <Windows.h>
#include <strsafe.h>
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"
UE_TRACE_CHANNEL_DEFINE(CyInsightsVulkanChannel);
UE_TRACE_EVENT_BEGIN(CyInsightsVulkan,Clock)
 UE_TRACE_EVENT_FIELD(uint64,GpuUs) UE_TRACE_EVENT_FIELD(uint64,CpuCycles) UE_TRACE_EVENT_FIELD(double,MaxErrorNs)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(CyInsightsVulkan,Pass)
 UE_TRACE_EVENT_FIELD(uint64,BeginUs) UE_TRACE_EVENT_FIELD(uint64,EndUs) UE_TRACE_EVENT_FIELD(uint64,ObservedCycles) UE_TRACE_EVENT_FIELD(uint64,Submission)
 UE_TRACE_EVENT_FIELD(uint32,Ordinal) UE_TRACE_EVENT_FIELD(uint32,Width) UE_TRACE_EVENT_FIELD(uint32,Height) UE_TRACE_EVENT_FIELD(uint32,Subpasses)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(CyInsightsVulkan,Contents)
 UE_TRACE_EVENT_FIELD(uint64,BeginUs) UE_TRACE_EVENT_FIELD(uint64,EndUs) UE_TRACE_EVENT_FIELD(uint64,PassId) UE_TRACE_EVENT_FIELD(uint64,Submission)
 UE_TRACE_EVENT_FIELD(uint32,Ordinal) UE_TRACE_EVENT_FIELD(uint32,Width) UE_TRACE_EVENT_FIELD(uint32,Height) UE_TRACE_EVENT_FIELD(uint32,SubpassIndex) UE_TRACE_EVENT_FIELD(uint32,Subpasses) UE_TRACE_EVENT_FIELD(uint32,Flags)
 UE_TRACE_EVENT_FIELD(uint64,DirectDraws) UE_TRACE_EVENT_FIELD(uint64,IndexedDraws) UE_TRACE_EVENT_FIELD(uint64,IndirectCalls) UE_TRACE_EVENT_FIELD(uint64,IndirectCommands)
 UE_TRACE_EVENT_FIELD(uint64,VerticesRequested) UE_TRACE_EVENT_FIELD(uint64,IndicesRequested) UE_TRACE_EVENT_FIELD(uint64,InstancesRequested)
UE_TRACE_EVENT_END()
int wmain(int argc,wchar_t** argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);if(argc!=2)return 2;FPlatformTime::InitTiming();
 UE::Trace::FInitializeDesc desc;desc.bUseWorkerThread=false;UE::Trace::Initialize(desc);UE::Trace::ToggleChannel(TEXT("CyInsightsVulkan"),true);UE::Trace::ToggleChannel(TEXT("cpu"),true);UE::Trace::ToggleChannel(TEXT("frame"),true);
 if(!UE::Trace::WriteTo(argv[1]))return 3;UE::Trace::Update();
 FMiscTrace::OutputBeginFrame(TraceFrameType_Game);FCpuProfilerTrace::OutputBeginDynamicEvent(TEXT("TEST FIXTURE CPU recording"));
 UE_TRACE_LOG(CyInsightsVulkan,Clock,CyInsightsVulkanChannel)<<Clock.GpuUs(5000000)<<Clock.CpuCycles(FPlatformTime::Cycles64())<<Clock.MaxErrorNs(1000.);
 Sleep(20);FCpuProfilerTrace::OutputEndEvent();FMiscTrace::OutputEndFrame(TraceFrameType_Game);FCpuProfilerTrace::FlushThreadBuffer();
 UE_TRACE_LOG(CyInsightsVulkan,Pass,CyInsightsVulkanChannel)<<Pass.BeginUs(5001000)<<Pass.EndUs(5014000)<<Pass.ObservedCycles(FPlatformTime::Cycles64())<<Pass.Submission(7)<<Pass.Ordinal(1)<<Pass.Width(2016)<<Pass.Height(1760)<<Pass.Subpasses(3);
 for(uint32 i=0;i<3;++i){UE_TRACE_LOG(CyInsightsVulkan,Contents,CyInsightsVulkanChannel)
  <<Contents.BeginUs(5001000)<<Contents.EndUs(5014000)<<Contents.PassId(21)<<Contents.Submission(7)<<Contents.Ordinal(1)<<Contents.Width(2016)<<Contents.Height(1760)<<Contents.SubpassIndex(i)<<Contents.Subpasses(3)<<Contents.Flags(i==2?2:0)
  <<Contents.DirectDraws(i==0?10:i==1?1:0)<<Contents.IndexedDraws(i==0?120:0)<<Contents.IndirectCalls(i==2?2:0)<<Contents.IndirectCommands(i==2?5:0)
  <<Contents.VerticesRequested(i==0?300:i==1?3:0)<<Contents.IndicesRequested(i==0?900000:0)<<Contents.InstancesRequested(i==0?150:i==1?1:0);}
 UE::Trace::Update();UE::Trace::Stop();UE::Trace::Shutdown();return 0;
}
